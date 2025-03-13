local function assert_colors_equal(c1, c2)
	if type(c1) == "table" and type(c2) == "table" then
		assert(c1.r == c2.r and c1.g == c2.g and c1.b == c2.b and c1.a == c2.a)
	else
		assert(c1 == c2)
	end
end

local function test_color_conversion()
	assert_colors_equal(core.colorspec_to_table("#fff"), {r = 255, g = 255, b = 255, a = 255})
	assert_colors_equal(core.colorspec_to_table(0xFF00FF00), {r = 0, g = 255, b = 0, a = 255})
	assert_colors_equal(core.colorspec_to_table("#00000000"), {r = 0, g = 0, b = 0, a = 0})
	assert_colors_equal(core.colorspec_to_table("green"), {r = 0, g = 128, b = 0, a = 255})
	assert_colors_equal(core.colorspec_to_table("gren"), nil)
end

unittests.register("test_color_conversion", test_color_conversion)
