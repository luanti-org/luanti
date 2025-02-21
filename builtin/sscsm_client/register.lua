local builtin_shared = ...

local make_registration = builtin_shared.make_registration

core.registered_globalsteps, core.register_globalstep = make_registration()
