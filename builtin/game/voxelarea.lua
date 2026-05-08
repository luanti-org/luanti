local math_floor = math.floor
local vector_new = vector.new

VoxelArea = {
	MinEdge = vector_new(1, 1, 1),
	MaxEdge = vector_new(0, 0, 0),
	ystride = 0,
	zstride = 0,
}

local class_metatable = {}
setmetatable(VoxelArea, class_metatable)

local function new(self, o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self

	local e = o:getExtent()
	o.ystride = e.x
	o.zstride = e.x * e.y

	return o
end

function class_metatable:__call(MinEdge, MaxEdge)
	return new(self, {MinEdge = MinEdge, MaxEdge = MaxEdge})
end

VoxelArea.new = new

function VoxelArea:getExtent()
	local MaxEdge, MinEdge = self.MaxEdge, self.MinEdge
	return vector_new(
		MaxEdge.x - MinEdge.x + 1,
		MaxEdge.y - MinEdge.y + 1,
		MaxEdge.z - MinEdge.z + 1
	)
end

function VoxelArea:getVolume()
	local e = self:getExtent()
	return e.x * e.y * e.z
end

function VoxelArea:index(x, y, z)
	local MinEdge = self.MinEdge
	local i = (z - MinEdge.z) * self.zstride +
			  (y - MinEdge.y) * self.ystride +
			  (x - MinEdge.x) + 1
	return math_floor(i)
end

function VoxelArea:indexp(p)
	local MinEdge = self.MinEdge
	local i = (p.z - MinEdge.z) * self.zstride +
			  (p.y - MinEdge.y) * self.ystride +
			  (p.x - MinEdge.x) + 1
	return math_floor(i)
end

function VoxelArea:position(i)
	local MinEdge = self.MinEdge

	i = i - 1

	local z = math_floor(i / self.zstride) + MinEdge.z
	i = i % self.zstride

	local y = math_floor(i / self.ystride) + MinEdge.y
	i = i % self.ystride

	local x = math_floor(i) + MinEdge.x

	return vector_new(x, y, z)
end

function VoxelArea:contains(x, y, z)
	local MaxEdge, MinEdge = self.MaxEdge, self.MinEdge
	return (x >= MinEdge.x) and (x <= MaxEdge.x) and
		   (y >= MinEdge.y) and (y <= MaxEdge.y) and
		   (z >= MinEdge.z) and (z <= MaxEdge.z)
end

function VoxelArea:containsp(p)
	local MaxEdge, MinEdge = self.MaxEdge, self.MinEdge
	return (p.x >= MinEdge.x) and (p.x <= MaxEdge.x) and
		   (p.y >= MinEdge.y) and (p.y <= MaxEdge.y) and
		   (p.z >= MinEdge.z) and (p.z <= MaxEdge.z)
end

function VoxelArea:containsi(i)
	return (i >= 1) and (i <= self:getVolume())
end

local function clamp_range(query_min, query_max, target_min, target_max)
	-- Return an invalid range if the requested range is empty,
	--     or doesn't intersect the target range.
	if query_min > query_max or query_max < target_min or query_min > target_max then
		-- Use fixed values for simplicity.
		return -1, -2
	end
	-- Otherwise, clamp the requested range to the target range.
	query_min = math.max(query_min, target_min)
	query_max = math.min(query_max, target_max)
	return query_min, query_max
end

function VoxelArea:iter(minx, miny, minz, maxx, maxy, maxz)
	minx, maxx = clamp_range(minx, maxx, self.MinEdge.x, self.MaxEdge.x)
	miny, maxy = clamp_range(miny, maxy, self.MinEdge.y, self.MaxEdge.y)
	minz, maxz = clamp_range(minz, maxz, self.MinEdge.z, self.MaxEdge.z)

	-- if the range is invalid or empty return an empty iterator
	if minx > maxx or miny > maxy or minz > maxz then
		return function()
			-- explicit nil for clear iterator protocol
			return nil
		end
	end

	local i = self:index(minx, miny, minz) - 1
	local xrange = maxx - minx + 1
	local nextaction = i + 1 + xrange

	local y = 0
	local yrange = maxy - miny + 1
	local yreqstride = self.ystride - xrange

	local z = 0
	local zrange = maxz - minz + 1
	local multistride = self.zstride - ((yrange - 1) * self.ystride + xrange)

	return function()
		-- continue i until it needs to jump
		i = i + 1
		if i ~= nextaction then
			return i
		end

		-- continue y until maxy is exceeded
		y = y + 1
		if y ~= yrange then
			-- set i to index(minx, miny + y, minz + z) - 1
			i = i + yreqstride
			nextaction = i + xrange
			return i
		end

		-- continue z until maxz is exceeded
		z = z + 1
		if z == zrange then
			-- cuboid finished, return nil
			return
		end

		-- set i to index(minx, miny, minz + z) - 1
		i = i + multistride

		y = 0
		nextaction = i + xrange
		return i
	end
end

function VoxelArea:iterp(minp, maxp)
	return self:iter(minp.x, minp.y, minp.z, maxp.x, maxp.y, maxp.z)
end
