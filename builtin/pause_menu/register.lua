local builtin_shared = ...

local make_registration = builtin_shared.make_registration

core.registered_on_formspec_input, core.register_on_formspec_input = make_registration()
