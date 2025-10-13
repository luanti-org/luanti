---@meta

---@class TileSpecColored
---@field name string
---@field color ColorSpec

---@class TileSpecFull
---@field name string
---@field backface_culling boolean?
---@field align_style TileAlignStyle?
---@field scale number?

---@alias TileAlignStyle
---| '"node"'
---| '"world"'
---| '"user"'

---@class TileSpecAnimation
---@field name string
---@field animation TileAnimationSpec

---@class TileAnimationSpec
---@field type TileAnimationType
---@field aspect_w number
---@field aspect_h number
---@field length number

---@alias TileAnimationType
---| "vertical_frames"
---| "sheet_2d"

---@alias TileSpec string|TileSpecColored|TileSpecFull|TileSpecAnimation
