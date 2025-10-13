---@alias NodeBoxAAABBB number[]

---@class NodeBoxRegular
---@field type '"regular"'

---@class NodeBoxFixed
---@field type '"fixed"'
---@field box NodeBoxAAABBB|NodeBoxAAABBB[]

---@class NodeBoxLeveled
---@field type '"leveled"'
---@field box NodeBoxAAABBB|NodeBoxAAABBB[]

---@class NodeBoxWallmounted
---@field type '"wallmounted"'
---@field wall_top NodeBoxAAABBB
---@field wall_bottom NodeBoxAAABBB
---@field wall_side NodeBoxAAABBB

---@class NodeBoxConnected
---@field type '"connected"'
---@field fixed NodeBoxAAABBB
---@field connect_top NodeBoxAAABBB?
---@field connect_bottom NodeBoxAAABBB?
---@field connect_front NodeBoxAAABBB?
---@field connect_left NodeBoxAAABBB?
---@field connect_back NodeBoxAAABBB?
---@field connect_right NodeBoxAAABBB?
---@field disconnected NodeBoxAAABBB?
---@field disconnected_top NodeBoxAAABBB?
---@field disconnected_bottom NodeBoxAAABBB?
---@field disconnected_front NodeBoxAAABBB?
---@field disconnected_left NodeBoxAAABBB?
---@field disconnected_back NodeBoxAAABBB?
---@field disconnected_right NodeBoxAAABBB?

---@alias NodeBoxSpec NodeBoxRegular|NodeBoxFixed|NodeBoxLeveled|NodeBoxWallmounted|NodeBoxConnected
