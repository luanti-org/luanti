core.log("info", "SSCSM testsscsm:init.lua loaded")
core.display_chat_message("hello from SSCSM testsscsm")

local helper_msg = dofile("helpers.lua")
core.display_chat_message("helpers.lua returned: " .. tostring(helper_msg))
