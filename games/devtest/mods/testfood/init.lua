local S = core.get_translator("testfood")

core.register_craftitem("testfood:good1", {
	description = S("Good Food (+1)").."\n"..
		S("Punch: Eat"),
	inventory_image = "testfood_good.png",
	on_use = core.item_eat(1),
})
core.register_craftitem("testfood:good5", {
	description = S("Good Food (+5)").."\n"..
		S("Punch: Eat"),
	inventory_image = "testfood_good2.png",
	on_use = core.item_eat(5),
})

core.register_craftitem("testfood:bad1", {
	description = S("Bad Food (-1)").."\n"..
		S("Punch: Eat"),
	inventory_image = "testfood_bad.png",
	on_use = core.item_eat(-1),
})
core.register_craftitem("testfood:bad5", {
	description = S("Bad Food (-5)").."\n"..
		S("Punch: Eat"),
	inventory_image = "testfood_bad2.png",
	on_use = core.item_eat(-5),
})

core.register_craftitem("testfood:replace1", {
	description = S("Replacing Food (+1)").."\n"..
		S("Punch: Eat and replace with 'Good Food (+1)'"),
	inventory_image = "testfood_replace.png",
	on_use = core.item_eat(1, "testfood:good1"),
})

