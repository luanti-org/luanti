--[[
    Math utils.
--]]

local POS_INF = math.huge
local NEG_INF = -math.huge

function math.hypot(x, y)
	return math.sqrt(x * x + y * y)
end

function math.sign(x, tolerance)
	tolerance = tolerance or 0
	if x > tolerance then
		return 1
	elseif x < -tolerance then
		return -1
	end
	return 0
end

function math.factorial(x)
	assert(x % 1 == 0 and x >= 0, "factorial expects a non-negative integer")
	if x >= 171 then
		-- 171! is greater than the biggest double, no need to calculate
		return POS_INF
	end
	local v = 1
	for k = 2, x do
		v = v * k
	end
	return v
end

function math.round(x)
	if x < 0 then
		local int = math.ceil(x)
		local frac = x - int
		return int - ((frac <= -0.5) and 1 or 0)
	end
	local int = math.floor(x)
	local frac = x - int
	return int + ((frac >= 0.5) and 1 or 0)
end

-- return `true` if `n` is neither an infinity nor a NaN and `false` otherwise
function math.isfinite(x)
	return (x < POS_INF) and (x > NEG_INF) -- will both be false if NaN
end
