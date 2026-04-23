local builtin_shared = ...

local make_registration = builtin_shared.make_registration

core.registered_globalsteps, core.register_globalstep = make_registration()
core.registered_on_modchannel_message, core.register_on_modchannel_message = make_registration()
core.registered_on_modchannel_signal, core.register_on_modchannel_signal = make_registration()
core.registered_on_clientmodchannel_message, core.register_on_clientmodchannel_message = make_registration()
core.registered_on_clientmodchannel_signal, core.register_on_clientmodchannel_signal = make_registration()
