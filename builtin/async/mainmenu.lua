core.log("info", "Initializing asynchronous environment")


function core.job_processor(func, serialized_param)
	local param = core.deserialize(serialized_param)

	local retval = core.serialize(func(param))

	return retval or core.serialize(nil)
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
