---@meta

---@class vector
---@field x number
---@field y number
---@field z number
---@operator add(vector): vector
---@operator sub(vector): vector
---@operator mul(vector): vector
---@operator div(vector): vector
---@operator unm: vector
local vector = {}


---Returns a new vector (a, b, c)
---@param a number
---@param b number?
---@param c number?
---@return vector
function vector.new(a, b, c) end


---Returns a new vector (0, 0, 0)
---@return vector
function vector.zero() end


---Returns a new vector of length 1, pointing into a direction chosen uniformly at random.
---@return vector
function vector.random_direction() end


---Returns a copy of the vector v.
---@param v vector
---@return vector
function vector.copy(v) end


---@return vector
function vector:copy() end


---@param s string
---@param init number?
---@return vector, number
function vector.from_string(s, init) end


---@param v vector
---@return string
function vector.to_string(v) end


---@return string
function vector:tostring() end


---@param pos1 vector
---@param pos2 vector
---@return vector
function vector.direction(pos1, pos2) end


---@param pos1 vector
---@param pos2 vector
---@return vector
function vector.distance(pos1, pos2) end


---@param v vector
---@return number
function vector.length(v) end


---@return number
function vector:length() end


---@param v vector
---@return vector
function vector.normalize(v) end


---@return vector
function vector:normalize() end


---@param v vector
---@return vector
function vector.floor(v) end


---@return vector
function vector:floor() end


---@param v vector
---@return vector
function vector.ceil(v) end


---@return vector
function vector:ceil() end


---@param v vector
---@return vector
function vector.round(v) end


---@return vector
function vector:round() end


---@param v vector
---@param tolerance number
---@return vector
function vector.sign(v, tolerance) end


---@param tolerance number
---@return vector
function vector:sign(tolerance) end


---@param v vector
---@return vector
function vector.abs(v) end


---@return vector
function vector:abs() end


---@param v vector
---@param func function
---@param ... any
---@return vector
function vector.apply(v, func, ...) end


---@param func function
---@param ... any
---@return vector
function vector:apply(func, ...) end


---@param v vector
---@param w vector
---@param func function(v: vector, w: vector): vector
---@return vector
function vector.combine(v, w, func) end


---@param w vector
---@param func function(v: vector, w: vector): vector
---@return vector
function vector:combine(w, func) end


---@param v1 vector
---@param v2 vector
---@return boolean
function vector.equals(v1, v2) end


---@param v vector
---@return boolean
function vector:equals(v) end


---@param v1 vector
---@param v2 vector
---@return vector, vector
function vector.sort(v1, v2) end


---@param v vector
---@return vector, vector
function vector:sort(v) end


---@param v1 vector
---@param v2 vector
---@return number
function vector.angle(v1, v2) end


---@param v1 vector
---@param v2 vector
---@return vector
function vector.dot(v1, v2) end


---@param v vector
---@return vector
function vector:dot(v) end


---@param v1 vector
---@param v2 vector
---@return vector
function vector.cross(v1, v2) end


---@param v vector
---@return vector
function vector:cross(v) end


---@param v vector
---@param x number
---@param y number
---@param z number
---@return vector
function vector.offset(v, x, y, z) end


---@param x number
---@param y number
---@param z number
---@return vector
function vector:offset(x, y, z) end


---@param v any
---@return boolean
function vector.check(v) end


---@param pos vector
---@param min vector
---@param max vector
---@return boolean
function vector.in_area(pos, min, max) end


---@param min vector
---@param max vector
---@return boolean
function vector:in_area(min, max) end


---@param min vector
---@param max vector
---@return vector
function vector.random_in_area(min, max) end


---@param v vector
---@param x vector|number
---@return vector
function vector.add(v, x) end


---@param x vector|number
---@return vector
function vector:add(x) end


---@param v vector
---@param x vector|number
---@return vector
function vector.substract(v, x) end


---@param x vector|number
---@return vector
function vector:substract(x) end


---@param v vector
---@param x vector|number
---@return vector
function vector.multiply(v, x) end


---@param x vector|number
---@return vector
function vector:multiply(x) end


---@param v vector
---@param x vector|number
---@return vector
function vector.divide(v, x) end


---@param x vector|number
---@return vector
function vector:divide(x) end


---@param v vector
---@param r vector
---@return vector
function vector.rotate(v, r) end


---@param r vector
---@return vector
function vector:rotate(r) end


---@param v1 vector
---@param v2 vector
---@param a number
---@return vector
function vector.rotate_around_axis(v1, v2, a) end


---@param direction vector
---@param up vector
---@return vector
function vector.dir_to_rotation(direction, up) end
