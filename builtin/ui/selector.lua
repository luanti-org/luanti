-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui._STATE_NONE = 0
ui._NUM_STATES = bit.lshift(1, 5)
ui._NO_STYLE = -1

local states_by_name = {
	focused  = bit.lshift(1, 0),
	selected = bit.lshift(1, 1),
	hovered  = bit.lshift(1, 2),
	pressed  = bit.lshift(1, 3),
	disabled = bit.lshift(1, 4),
}

--[[
Selector parsing functions return a function. When called with an element as
the solitary parameter, this function will return a boolean, indicating whether
the element is matched by the selector. If the boolean is true, a table of
tables {box=..., states=...} is also returned. If false, this is nil.

The keys of this table are unique hashes of the box, which serve to prevent
duplicate box/state combos from being generated. The values contain all the
combinations of boxes and states that the selector specifies. The box name may
be nil if the selector specified no box, in which case it will default to
"main" unless/until it is later intersected with a box selector. This list may
also be empty, which means that contradictory boxes were specified and no box
should be styled. The list will not contain duplicates.
--]]

-- By default, most selectors leave the box unspecified and don't select any
-- particular state, leaving the state at zero.
local function make_box(name, states)
	return {name = name, states = states or ui._STATE_NONE}
end

-- Hash the box to a string that represents that combination of box and states
-- uniquely to prevent duplicates in box tables.
local function hash_box(box)
	return (box.name or "") .. "$" .. tostring(box.states)
end

local function make_hashed(name, states)
	local box = make_box(name, states)
	return {[hash_box(box)] = box}
end

local function result(matches, name, states)
	if matches then
		return true, make_hashed(name, states)
	end
	return false, nil
end

ui._universal_sel = function()
	return result(true)
end

local simple_preds = {
	["empty"] = function(elem)
		return result(#elem._children == 0)
	end,

	["first_child"] = function(elem)
		return result(elem._parent == nil or elem._index == 1)
	end,

	["last_child"] = function(elem)
		return result(elem._parent == nil or elem._rindex == 1)
	end,

	["only_child"] = function(elem)
		return result(elem._parent == nil or #elem._parent._children == 1)
	end,
}

local sel_preds = {
	["<"] = function(sel)
		return function(elem)
			return result(elem._parent and sel(elem._parent))
		end
	end,

	[">"] = function(sel)
		return function(elem)
			for _, child in ipairs(elem._children) do
				if sel(child) then
					return result(true)
				end
			end
			return result(false)
		end
	end,

	["<<"] = function(sel)
		return function(elem)
			local ancestor = elem._parent

			while ancestor ~= nil do
				if sel(ancestor) then
					return result(true)
				end
				ancestor = ancestor._parent
			end

			return result(false)
		end
	end,

	[">>"] = function(sel)
		return function(elem)
			for _, descendant in ipairs(elem:_get_flat()) do
				if descendant ~= elem and sel(descendant) then
					return result(true)
				end
			end
			return result(false)
		end
	end,

	["<>"] = function(sel)
		return function(elem)
			if not elem._parent then
				return result(false)
			end

			for _, sibling in ipairs(elem._parent._children) do
				if sibling ~= elem and sel(sibling) then
					return result(true)
				end
			end

			return result(false)
		end
	end,
}

local special_preds = {
	["nth_child"] = function(str)
		local index = tonumber(str)
		assert(index, "Expected number for ?nth_child")

		return function(elem)
			if not elem._parent then
				return result(index == 1)
			end
			return result(elem._index == index)
		end
	end,

	["nth_last_child"] = function(str)
		local rindex = tonumber(str)
		assert(rindex, "Expected number for ?nth_last_child")

		return function(elem)
			if not elem._parent then
				return result(rindex == 1)
			end
			return result(elem._rindex == rindex)
		end
	end,
}

local function parse_term(str, pred)
	str = str:trim()
	assert(str ~= "", "Expected selector term")

	-- We need to test the first character to see what sort of term we're
	-- dealing with, and then usually parse from the rest of the string.
	local prefix = str:sub(1, 1)
	local suffix = str:sub(2)

	if prefix == "*" then
		-- Universal terms match everything and have no extra stuff to parse.
		return suffix, ui._universal_sel

	elseif prefix == "#" then
		-- Most selectors are similar to the ID selector, in that characters
		-- for the ID string are parsed, and all the characters directly
		-- afterwards are returned as the rest of the string after the term.
		local id, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(id, "Expected ID after '#'")

		return rest, function(elem)
			return result(elem._id == id)
		end

	elseif prefix == "." then
		local group, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(group, "Expected group after '.'")

		return rest, function(elem)
			return result(elem._groups[group] ~= nil)
		end

	elseif prefix == "@" then
		-- It's possible to check if a box exists in a predicate, but that
		-- leads to different behaviors inside and outside of predicates. For
		-- instance, @main@thumb effectively matches nothing by returning an
		-- empty table of boxes, but would return true for scrollbars if used
		-- in a predicate. So, prevent box selectors in predicates entirely.
		assert(not pred, "Box selectors are invalid for predicates")

		local name, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(name, "Expected box after '@'")

		return rest, function(elem)
			if elem._boxes[name] then
				-- If the box is in the element, return it.
				return result(true, name, ui._STATE_NONE)
			elseif name == "all" then
				-- If we want all boxes, iterate over the boxes in the element
				-- and add each of them to a full list of boxes.
				local boxes = {}

				for name in pairs(elem._boxes) do
					local box = make_box(name, ui._STATE_NONE)
					boxes[hash_box(box)] = box
				end

				return true, boxes
			end

			-- Otherwise, the selector doesn't match.
			return result(false)
		end

	elseif prefix == "$" then
		-- Unfortunately, we can't detect the state of boxes from the server,
		-- so we can't use them in predicates.
		assert(not pred, "Style selectors are invalid for predicates")

		local name, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(name, "Expected state after '$'")

		local state = states_by_name[name]
		assert(state, "Invalid state: '" .. name .. "'")

		return rest, function(elem)
			-- States unconditionally match every element. Specify the state
			-- that this term indicates but leave the box undefined.
			return result(true, nil, state)
		end

	elseif prefix == "/" then
		local type, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)%/(.*)$")
		assert(type, "Expected window type after '/'")

		assert(ui._window_types[type], "Invalid window type: '" .. type .. "'")

		return rest, function(elem)
			return result(elem._window._type == type)
		end

	elseif prefix == "," then
		-- Since we don't know which terms came directly behind us, we return
		-- nil instead of returning a selector function so that ui._parse_sel()
		-- can union the two selectors on either side of the comma.
		return suffix, nil

	elseif prefix == "(" then
		-- Parse a matching set of parentheses, and recursively pass the
		-- contents into ui._parse_sel().
		local sub, rest = str:match("^(%b())(.*)$")
		assert(sub, "Unmatched ')' for '('")

		return rest, ui._parse_sel(sub:sub(2, -2), pred)

	elseif prefix == "!" then
		-- Parse a single predicate term (NOT an entire predicate selector) and
		-- ensure that it's a valid selector term, not a comma.
		local rest, term = parse_term(suffix, true)
		assert(term, "Expected selector term after '!'")

		return rest, function(elem)
			return result(not term(elem))
		end

	elseif prefix == "?" then
		-- Predicates may have different syntax depending on the name of the
		-- predicate, so just parse the name initially.
		local name, after = suffix:match("^([" .. ui._ID_CHARS .. "%<%>]+)(.*)$")
		assert(name, "Expected predicate after '?'")

		-- If this is a simple predicate, return its predicate function without
		-- doing any further parsing.
		local func = simple_preds[name]
		if func then
			return after, func
		end

		-- If this is a function predicate, we need to do more parsing.
		func = sel_preds[name] or special_preds[name]
		if func then
			-- Parse a matching pair of parentheses and get the contents
			-- between them.
			assert(after:sub(1, 1) == "(", "Expected '(' after '?" .. name .. "'")

			local sub, rest = after:match("^(%b())(.*)$")
			assert(sub, "Unmatched ')' for '?" .. name .. "('")

			local contents = sub:sub(2, -2)

			-- If this is a function predicate that wants a selector, parse the
			-- contents as a predicate selector and pass it on to the selector
			-- creation function.
			if sel_preds[name] then
				return rest, func(ui._parse_sel(contents, true))
			end

			-- Otherwise, hand the string directly to the function for special
			-- processing, which we automatically trim for convenience.
			return rest, func(contents:trim())
		end

		-- Otherwise, there is no predicate by this name.
		error("Invalid predicate: '?" .. name .. "'")

	else
		-- If we found no special character, it's either a type or it indicates
		-- invalid characters in the selector string.
		local type, rest = str:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(type, "Unexpected character in selector: '" .. prefix .. "'")

		assert(ui._elem_types[type], "Invalid element type: '" .. type .. "'")

		return rest, function(elem)
			return result(elem._type == type)
		end
	end
end

local function intersect_boxes(a_boxes, b_boxes)
	local new_boxes = {}

	for _, box_a in pairs(a_boxes) do
		for _, box_b in pairs(b_boxes) do
			-- Two boxes can only be merged if they're the same box or if one
			-- or both selectors hasn't specified a box yet.
			if box_a.name == nil or box_b.name == nil or box_a.name == box_b.name then
				-- Create the new box by taking the specified box (if there is
				-- one) and ORing the states together (making them more refer
				-- to a more specific state).
				local new_box = make_box(
					box_a.name or box_b.name,
					bit.bor(box_a.states, box_b.states)
				)

				-- Hash this box and add it into the table. This will be
				-- effectively a no-op if there's already an identical box
				-- hashed in the table.
				new_boxes[hash_box(new_box)] = new_box
			end
		end
	end

	return new_boxes
end

function ui._intersect_sels(sels)
	return function(elem)
		-- We start with the default box, and intersect the box and states from
		-- every selector with it.
		local all_boxes = make_hashed()

		-- Loop through all of the selectors. All of them need to match for the
		-- intersected selector to match.
		for _, sel in ipairs(sels) do
			local matches, boxes = sel(elem)
			if not matches then
				-- This selector doesn't match, so fail immediately.
				return false, nil
			end

			-- Since the selector matched, intersect the boxes and states with
			-- those of the other selectors. If two selectors both match an
			-- element but specify different boxes, then this selector will
			-- return true, but the boxes will be cancelled out in the
			-- intersection, leaving an empty list of boxes.
			if boxes then
				all_boxes = intersect_boxes(all_boxes, boxes)
			end
		end

		return true, all_boxes
	end
end

function ui._union_sels(sels)
	return function(elem)
		-- We initially have no boxes, and have to add them in as matching
		-- selectors are unioned in.
		local all_boxes = {}
		local found_match = false

		-- Loop through all of the selectors. If any of them match, this entire
		-- unioned selector matches.
		for _, sel in ipairs(sels) do
			local matches, boxes = sel(elem)

			if matches then
				-- We found a match. However, we can't return true just yet
				-- because we need to union the boxes and states from every
				-- selector, not just this one.
				found_match = true

				if boxes then
					-- Add the boxes from this selector into the table of all
					-- the boxes. The hashing of boxes will automatically weed
					-- out any duplicates.
					for hash, box in pairs(boxes) do
						all_boxes[hash] = box
					end
				end
			end
		end

		if found_match then
			return true, all_boxes
		end
		return false, nil
	end
end

function ui._parse_sel(str, pred)
	str = str:trim()
	assert(str ~= "", "Empty style selector")

	local sub_sels = {}
	local terms = {}

	-- Loop until we've read every term from the input string.
	repeat
		-- Parse the next term from the input string.
		local term
		str, term = parse_term(str, pred)

		if term ~= nil then
			-- If we didn't read a comma, insert this term into the list of
			-- terms for the current sub-selector.
			table.insert(terms, term)
		else
			-- If we read a comma, make sure that we have terms before and
			-- after it so it's not dangling.
			assert(#terms > 0, "Expected selector term before ','")
			assert(str ~= "", "Expected selector term after ','")
		end

		-- If we read a comma or ran out of terms, we need to commit the terms
		-- we've read so far.
		if term == nil or str == "" then
			-- If there's only one term, commit it directly. Otherwise,
			-- intersect all the terms together.
			if #terms == 1 then
				table.insert(sub_sels, terms[1])
			else
				table.insert(sub_sels, ui._intersect_sels(terms))
			end

			-- Clear out the list of terms for the next sub-selector.
			terms = {}
		end
	until str == ""

	-- Now that we've read all the sub-selectors between the commas, we need to
	-- commit them. We only need to union the terms if there's more than one.
	if #sub_sels == 1 then
		return sub_sels[1]
	end
	return ui._union_sels(sub_sels)
end
