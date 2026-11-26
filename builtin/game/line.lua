-- Line util helpers, for use in the
-- core.add_line / ObjectRef:set_points()

core.line_helpers = core.line_helpers or {}

-- Parabola, not catenary since its cheaper to compute and solve is 100% guaranteed
-- @param p1 a vector for first endpoint of Parabola
-- @param p2 a vector for 2nd endpoint of parabola
-- @param sag Optional, default of 0. How 'droopy' the parabola should look
-- @param segments Optional, default to 10, how many line segments to generate
function core.line_helpers.parabola_points(p1, p2, sag, segments)
	segments = segments or 10
	sag = sag or 0

	local points = {}
	for i = 0, segments do
		local t = i / segments
		local drop = 4 * sag * t * (1 - t)
		points[i + 1] = {pos = vector.new(
			p1.x + (p2.x - p1.x) * t,
			p1.y + (p2.y - p1.y) * t - drop,
			p1.z + (p2.z - p1.z) * t
		)}
	end

	return points
end
