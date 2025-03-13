local S, NS = core.get_translator("testtranslations")

local function send_compare(name, text)
	core.chat_send_player(name, ("%s | %s | %s"):format(
			core.get_translated_string("", text), text, core.get_translated_string("fr", text)))
end

core.register_chatcommand("testtranslations", {
	params = "",
	description = "Test translations",
	privs = {},
	func = function(name, param)
		core.chat_send_player(name, "Please ensure your locale is set to \"fr\"")
		core.chat_send_player(name, "Untranslated | Client-side translation | Server-side translation (fr)")
		send_compare(name, S("Testing .tr files: untranslated"))
		send_compare(name, S("Testing .po files: untranslated"))
		send_compare(name, S("Testing .mo files: untranslated"))
		send_compare(name, S("Testing fuzzy .po entry: untranslated (expected)"))
		send_compare(name, core.translate("translation_po", "Testing .po without context: untranslated"))
		send_compare(name, core.translate("translation_mo", "Testing .mo without context: untranslated"))
		for i = 0,4 do
			send_compare(name, NS("@1: .po singular", "@1: .po plural", i, tostring(i)))
			send_compare(name, NS("@1: .mo singular", "@1: .mo plural", i, tostring(i)))
		end
	end
})
