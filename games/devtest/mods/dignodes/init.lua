local groups = {
	"cracky", "dig_immediate"
}

-- Register dig nodes with 1 digging group, a rating between 1-3 and a level between 0-2
for g=1, #groups do
	local gr = groups[g]
	for r=1, 3 do
		for l=0, 2 do
			if not (gr=="dig_immediate" and (l>0 or r==1)) then
				local d
				if l > 0 then
					d = string.format("Dig Test Node: %s=%d, level=%d", gr, r, l)
				else
					d = string.format("Dig Test Node: %s=%d", gr, r)
				end
				local tile = "dignodes_"..gr..".png^dignodes_rating"..r..".png"
				if l==1 then
					tile = tile .. "^[colorize:#FFFF00:127"
				elseif l==2 then
					tile = tile .. "^[colorize:#FF0000:127"
				end
				core.register_node("dignodes:"..gr.."_"..r.."_"..l, {
					description = d,
					tiles = { tile },
					groups = { [gr] = r, level = l },
				})
			end
		end
	end
end

-- Node without any digging groups
core.register_node("dignodes:none", {
	description = "Dig Test Node: groupless".."\n"..
		"Can't be dug by normal digging tools".."\n"..
		"(use the Remover tool to remove)",
	tiles = {"dignodes_none.png"},
})
