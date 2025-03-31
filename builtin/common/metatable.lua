-- Registered metatables, used by the C++ packer
local serializable_metatables = {}
local known_metatables = {}

local function dummy_serializer(x)
	return x
end

function core.register_portable_metatable(name, mt, serializer, deserializer)
	serializer = serializer or dummy_serializer
	deserializer = deserializer or function(x) return setmetatable(x, mt) end
	assert(type(name) == "string", ("attempt to use %s value as metatable name"):format(type(name)))
	assert(type(mt) == "table", ("attempt to register a %s value as metatable"):format(type(mt)))
	assert(type(serializer), ("attempt to use a %s value as serializer"):format(type(serializer)))
	assert(type(deserializer), ("attempt to use a %s value as serialier"):format(type(deserializer)))
	assert(known_metatables[name] == nil or known_metatables[name] == mt,
			("attempt to override metatable %s"):format(name))
	known_metatables[name] = mt
	known_metatables[mt] = name
	serializable_metatables[mt] = serializer
	serializable_metatables[name] = deserializer
end

core.known_metatables = known_metatables
core.serializable_metatables = serializable_metatables

function core.register_async_metatable(...)
	core.log("deprecated", "core.register_async_metatable is deprecated. " ..
			"Use core.register_portable_metatable instead.")
	return core.register_portable_metatable(...)
end

core.register_portable_metatable("__builtin:vector", vector.metatable)

if ItemStack then
	local item = ItemStack()
	local itemstack_mt = getmetatable(item)
	core.register_portable_metatable("__itemstack", itemstack_mt, item.to_table, ItemStack)
end
