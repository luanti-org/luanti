core.log("info", "Initializing asynchronous environment")

-- Entrypoint to run async jobs, called by C++
function core.job_processor(func, param)
	local retval = func(param)

	return retval
end

function core.get_http_accept_languages()
	local languages
	local current_language = core.get_language()
	if current_language ~= "" then
		languages = { current_language, "en;q=0.8" }
	else
		languages = { "en" }
	end
	return "Accept-Language: " .. table.concat(languages, ", ")
end
