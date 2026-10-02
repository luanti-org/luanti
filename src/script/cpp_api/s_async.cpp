// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier, <sapier AT gmx DOT net>

#include <cstdio>
#include <cstdlib>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "s_async.h"
#include "log.h"
#include "settings.h"
#include "porting.h"
#include "filesys.h"
#include "util/thread.h"
#include "common/c_internal.h"
#include "common/c_packer.h"
#include "lua_api/l_base.h"

// if a job is waiting for this duration, an additional thread will be spawned
static constexpr int AUTOSCALE_DELAY_MS = 1000;
// if jobs are waiting for this duration, a warning is printed
static constexpr int STUCK_DELAY_MS = 11500;

/******************************************************************************/
AsyncEngine::~AsyncEngine()
{
	// Request all threads to stop
	for (AsyncWorkerThread *workerThread : workerThreads) {
		workerThread->stop();
	}

	// Wake up all threads
	for (auto it : workerThreads) {
		(void)it;
		jobQueueCounter.post();
	}

	// Wait for threads to finish
	infostream << "AsyncEngine: Waiting for " << workerThreads.size()
		<< " threads" << std::endl;
	for (AsyncWorkerThread *workerThread : workerThreads) {
		workerThread->wait();
	}

	for (AsyncWorkerThread *workerThread : workerThreads) {
		delete workerThread;
	}

	jobQueueMutex.lock();
	jobQueue.clear();
	jobQueueMutex.unlock();
	workerThreads.clear();
}

/******************************************************************************/
void AsyncEngine::registerStateInitializer(StateInitializer func)
{
	FATAL_ERROR_IF(initDone, "Initializer may not be registered after init");
	stateInitializers.push_back(func);
}

/******************************************************************************/
void AsyncEngine::initialize(unsigned int numEngines, const char *initType, bool enableSecurity)
{
	FATAL_ERROR_IF(initDone, "Already initialized");
	initDone = true;

	assert(parent && initType);
	this->initType = initType;
	this->enableSecurity = enableSecurity;

	if (numEngines == 0) {
		// Leave one core for the main thread and one for whatever else
		autoscaleMaxWorkers = Thread::getNumberOfProcessors();
		if (autoscaleMaxWorkers >= 2)
			autoscaleMaxWorkers -= 2;
		infostream << "AsyncEngine: using at most " << autoscaleMaxWorkers
			<< " threads with automatic scaling" << std::endl;

		addWorkerThread();
	} else {
		for (unsigned int i = 0; i < numEngines; i++)
			addWorkerThread();
	}
}

void AsyncEngine::addWorkerThread()
{
	AsyncWorkerThread *toAdd = new AsyncWorkerThread(this,
		std::string("AsyncWorker-") + std::to_string(workerThreads.size()));
	workerThreads.push_back(toAdd);
	toAdd->start();
}

/******************************************************************************/

u32 AsyncEngine::queueAsyncJob(LuaJobInfo &&job)
{
	MutexAutoLock autolock(jobQueueMutex);
	u32 jobId = jobIdCounter++;

	assert(!job.function.empty());
	job.id = jobId;
	jobQueue.push_back(std::move(job));

	jobQueueCounter.post();
	return jobId;
}

u32 AsyncEngine::queueAsyncJob(std::string &&func, PackedValue *params,
		const std::string &mod_origin)
{
	LuaJobInfo to_add(std::move(func), params, mod_origin);
	return queueAsyncJob(std::move(to_add));
}

bool AsyncEngine::cancelAsyncJob(u32 id)
{
	MutexAutoLock autolock(jobQueueMutex);
	for (auto job = jobQueue.begin(); job != jobQueue.end(); job++) {
		if (job->id == id) {
			jobQueue.erase(job);
			return true;
		}
	}
	return false;
}

/******************************************************************************/
bool AsyncEngine::getJob(LuaJobInfo *job)
{
	jobQueueCounter.wait();
	jobQueueMutex.lock();

	bool retval = false;

	if (!jobQueue.empty()) {
		*job = std::move(jobQueue.front());
		jobQueue.pop_front();
		retval = true;
	}
	jobQueueMutex.unlock();

	return retval;
}

/******************************************************************************/
void AsyncEngine::putJobResult(LuaJobInfo &&result)
{
	resultQueueMutex.lock();
	resultQueue.emplace_back(std::move(result));
	resultQueueMutex.unlock();
}

/******************************************************************************/
void AsyncEngine::step(lua_State *L)
{
	stepJobResults(L);
	stepAutoscale();
	stepStuckWarning();
}

void AsyncEngine::stepJobResults(lua_State *L)
{
	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");

	ScriptApiBase *script = ModApiBase::getScriptApiBase(L);

	MutexAutoLock autolock(resultQueueMutex);
	while (!resultQueue.empty()) {
		LuaJobInfo j = std::move(resultQueue.front());
		resultQueue.pop_front();

		lua_getfield(L, -1, "async_event_handler");
		if (lua_isnil(L, -1))
			FATAL_ERROR("Async event handler does not exist!");
		luaL_checktype(L, -1, LUA_TFUNCTION);

		lua_pushinteger(L, j.id);
		script_unpack(L, j.result_ext.get());

		// Call handler
		const char *origin = j.mod_origin.empty() ? nullptr : j.mod_origin.c_str();
		script->setOriginDirect(origin);
		int result = lua_pcall(L, 2, 0, error_handler);
		if (result)
			script_error(L, result, origin, "<async>");
	}

	lua_pop(L, 2); // Pop core and error handler
}

void AsyncEngine::stepAutoscale()
{
	if (workerThreads.size() >= autoscaleMaxWorkers)
		return;

	MutexAutoLock autolock(jobQueueMutex);

	// 2) If the timer elapsed, check again
	if (autoscaleTimer && porting::getTimeMs() >= autoscaleTimer) {
		autoscaleTimer = 0;
		// Determine overlap with previous snapshot
		size_t n = compareJobs(autoscaleSeenJobs);
		infostream << "AsyncEngine: " << n << " jobs were still waiting after "
			<< AUTOSCALE_DELAY_MS << "ms, adding more threads." << std::endl;
		// Start this many new threads
		while (workerThreads.size() < autoscaleMaxWorkers && n > 0) {
			addWorkerThread();
			n--;
		}
		return;
	}

	// 1) Check queue contents
	if (!autoscaleTimer && !jobQueue.empty()) {
		autoscaleSeenJobs.clear();
		snapshotJobs(autoscaleSeenJobs);
		autoscaleTimer = porting::getTimeMs() + AUTOSCALE_DELAY_MS;
	}
}

void AsyncEngine::stepStuckWarning()
{
	MutexAutoLock autolock(jobQueueMutex);

	// 2) If the timer elapsed, check again
	if (stuckTimer && porting::getTimeMs() >= stuckTimer) {
		stuckTimer = 0;
		size_t n = compareJobs(stuckSeenJobs);
		if (n > 0) {
			warningstream << "AsyncEngine: " << n << " jobs seem to be stuck in queue"
				" (" << workerThreads.size() << " workers active)" << std::endl;
		}
		// fallthrough
	}

	// 1) Check queue contents
	if (!stuckTimer && !jobQueue.empty()) {
		stuckSeenJobs.clear();
		snapshotJobs(stuckSeenJobs);
		stuckTimer = porting::getTimeMs() + STUCK_DELAY_MS;
	}
}

/******************************************************************************/
bool AsyncEngine::prepareEnvironment(lua_State* L, int top)
{
	for (const auto &init : stateInitializers) {
		init(L, top);
	}

	lua_pushstring(L, initType);
	lua_setglobal(L, "INIT");

	auto *script = ModApiBase::getScriptApiBase(L);
	try {
		script->loadMod(ScriptApiBase::getBuiltinLuaPath() + DIR_DELIM "init.lua",
			BUILTIN_MOD_NAME);
		script->checkSetByBuiltin();
	} catch (const ModError &e) {
		errorstream << "Execution of async base environment failed: "
			<< e.what() << std::endl;
		parent->reportAsyncError(e.what());
		return false;
	}

	return parent->onAsyncEnvSetup(script);
}

AsyncWorkerThread::AsyncWorkerThread(AsyncEngine* jobDispatcher,
		const std::string &name) :
	ScriptApiBase(ScriptingType::Async),
	Thread(name),
	jobDispatcher(jobDispatcher)
{
	lua_State *L = getStack();

	// Inherit gamedef from parent
	if (jobDispatcher->parent->getGameDef())
		setGameDef(jobDispatcher->parent->getGameDef());

	if (jobDispatcher->enableSecurity)
		initializeSecurity();
	else
		infostream << "AsyncWorkerThread: mod security is disabled!" << std::endl;

	// Prepare job lua environment
	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	if (!jobDispatcher->prepareEnvironment(L, top)) {
		// can't throw from here so we're stuck with this
		isErrored = true;
	}
	lua_pop(L, 1);
}

AsyncWorkerThread::~AsyncWorkerThread()
{
	sanity_check(!isRunning());
}

bool AsyncWorkerThread::checkPathInternal(const std::string &abs_path,
	bool write_required, bool *write_allowed)
{
	return jobDispatcher->parent->checkPathInternal(abs_path, write_required, write_allowed);
}

void* AsyncWorkerThread::run()
{
	if (isErrored)
		return nullptr;

	lua_State *L = getStack();

	int error_handler = PUSH_ERROR_HANDLER(L);

	auto report_error = [this] (const ModError &e) {
		jobDispatcher->parent->reportAsyncError(e.what());
	};

	lua_getglobal(L, "core");
	if (lua_isnil(L, -1)) {
		FATAL_ERROR("Unable to find core within async environment!");
	}

	// Main loop
	LuaJobInfo j;
	while (!stopRequested()) {
		// Wait for job
		if (!jobDispatcher->getJob(&j) || stopRequested())
			continue;

		lua_getfield(L, -1, "job_processor");
		if (lua_isnil(L, -1))
			FATAL_ERROR("Unable to get async job processor!");
		luaL_checktype(L, -1, LUA_TFUNCTION);

		if (luaL_loadbuffer(L, j.function.data(), j.function.size(), "=(async)")) {
			errorstream << "ASYNC WORKER: Unable to deserialize function" << std::endl;
			lua_pushnil(L);
		}
		script_unpack(L, j.params_ext.get());

		// Call it
		setOriginDirect(j.mod_origin.empty() ? nullptr : j.mod_origin.c_str());
		int result = lua_pcall(L, 2, 1, error_handler);
		if (result) {
			try {
				scriptError(result, "<async>");
			} catch (const ModError &e) {
				report_error(e);
			}
		} else {
			// Fetch result
			try {
				j.result_ext.reset(script_pack(L, -1));
			} catch (const ModError &e) {
				report_error(e);
				result = LUA_ERRERR;
			}
		}

		lua_pop(L, 1);  // Pop retval

		// Put job result
		if (result == 0)
			jobDispatcher->putJobResult(std::move(j));
	}

	lua_pop(L, 2);  // Pop core and error handler

	return 0;
}

u32 ScriptApiAsync::queueAsync(std::string &&serialized_func,
		PackedValue *param, const std::string &mod_origin)
{
	return asyncEngine.queueAsyncJob(std::move(serialized_func),
			param, mod_origin);
}

bool ScriptApiAsync::cancelAsync(u32 id)
{
	return asyncEngine.cancelAsyncJob(id);
}

void ScriptApiAsync::stepAsync()
{
	asyncEngine.step(getStack());
}
