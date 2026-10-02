
core.async_jobs = {}

function core.async_event_handler(jobid, retval)
	local callback = core.async_jobs[jobid]
	assert(type(callback) == "function")
	callback(retval)
	core.async_jobs[jobid] = nil
end

function core.handle_async(func, parameter, callback)
	local jobid = core.do_async_callback(func, parameter)

	core.async_jobs[jobid] = callback

	return true
end

