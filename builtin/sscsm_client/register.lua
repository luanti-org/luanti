local builtin_shared = ...

local make_registration = builtin_shared.make_registration

core.registered_globalsteps, core.register_globalstep = make_registration()
core.registered_on_chatcommands, core.register_on_chatcommand = make_registration()
