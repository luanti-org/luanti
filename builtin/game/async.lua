
core.async_jobs = {}

function core.async_event_handler(jobid, retval)
	local callback = core.async_jobs[jobid]
	assert(type(callback) == "function")
	callback(unpack(retval, 1, retval.n))
	core.async_jobs[jobid] = nil
end

local job_metatable = {__index = {}}

function job_metatable.__index:cancel()
	local cancelled = core.cancel_async_callback(self.id)
	if cancelled then
		core.async_jobs[self.id] = nil
	end
	return cancelled
end

function core.handle_async(func, callback, ...)
	assert(type(func) == "function" and type(callback) == "function",
		"Invalid core.handle_async invocation")
	local args = {n = select("#", ...), ...}
	local mod_origin = core.get_last_run_mod()

	local id = core.do_async_callback(func, args, mod_origin)
	core.async_jobs[id] = callback

	return setmetatable({id = id}, job_metatable)
end
