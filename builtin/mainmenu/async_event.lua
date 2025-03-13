
core.async_jobs = {}

local function handle_job(jobid, serialized_retval)
	local retval = core.deserialize(serialized_retval)
	assert(type(core.async_jobs[jobid]) == "function")
	core.async_jobs[jobid](retval)
	core.async_jobs[jobid] = nil
end

core.async_event_handler = handle_job

function core.handle_async(func, parameter, callback)
	-- Serialize parameters
	local serialized_param = core.serialize(parameter)

	if serialized_param == nil then
		return false
	end

	local jobid = core.do_async_callback(func, serialized_param)

	core.async_jobs[jobid] = callback

	return true
end

