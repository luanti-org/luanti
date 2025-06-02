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

local simple_preds = {}
local func_preds = {}

simple_preds["no_children"] = function(elem)
	return result(#elem._children == 0)
end

simple_preds["first_child"] = function(elem)
	return result(elem._parent == nil or elem._parent._children[1] == elem)
end

simple_preds["last_child"] = function(elem)
	return result(elem._parent == nil or
			elem._parent._children[#elem._parent._children] == elem)
end

simple_preds["only_child"] = function(elem)
	return result(elem._parent == nil or #elem._parent._children == 1)
end

func_preds["<"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		return result(elem._parent and sel(elem._parent))
	end
end

func_preds[">"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		for _, child in ipairs(elem._children) do
			if sel(child) then
				return result(true)
			end
		end
		return result(false)
	end
end

func_preds["<<"] = function(str)
	local sel = ui._parse_sel(str, true, false)

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
end

func_preds[">>"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		for _, descendant in ipairs(elem:_get_flat()) do
			if descendant ~= elem and sel(descendant) then
				return result(true)
			end
		end
		return result(false)
	end
end

func_preds["<>"] = function(str)
	local sel = ui._parse_sel(str, true, false)

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
end

func_preds["nth_child"] = function(str)
	local index = tonumber(str)
	assert(index, "Expected number for ?nth_child()")

	return function(elem)
		if not elem._parent then
			return result(index == 1)
		end
		return result(elem._parent._children[index] == elem)
	end
end

func_preds["nth_last_child"] = function(str)
	local rindex = tonumber(str)
	assert(rindex, "Expected number for ?nth_last_child()")

	return function(elem)
		if not elem._parent then
			return result(rindex == 1)
		end

		local index = #elem._parent._children - rindex + 1
		return result(elem._parent._children[index] == elem)
	end
end

local function is_nth_match(elem, sel, index, dir)
	if not elem._parent then
		return index == 1 and sel(elem)
	end

	local first, last
	if dir == 1 then
		first = 1
		last = #elem._parent._children
	else
		first = #elem._parent._children
		last = 1
	end

	local count = 0
	for i = first, last, dir do
		local sibling = elem._parent._children[i]

		if sel(sibling) then
			count = count + 1
		end

		if count == index then
			return sibling == elem
		end
	end

	return false
end

func_preds["first_match"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		return is_nth_match(elem, sel, 1, 1)
	end
end

func_preds["last_match"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		return is_nth_match(elem, sel, 1, -1)
	end
end

func_preds["only_match"] = function(str)
	local sel = ui._parse_sel(str, true, false)

	return function(elem)
		return is_nth_match(elem, sel, 1, 1) and is_nth_match(elem, sel, 1, -1)
	end
end

func_preds["nth_match"] = function(str)
	local sel, rest = ui._parse_sel(str, true, true)
	local index = tonumber(rest)
	assert(index, "Expected number after ';' for ?nth_match()")

	return function(elem)
		return is_nth_match(elem, sel, index, 1)
	end
end

func_preds["nth_last_match"] = function(str)
	local sel, rest = ui._parse_sel(str, true, true)
	local rindex = tonumber(rest)
	assert(rindex, "Expected number after ';' for ?nth_last_match()")

	return function(elem)
		return is_nth_match(elem, sel, rindex, -1)
	end
end

func_preds["family"] = function(family)
	if family == "*" then
		return function(elem)
			return result(elem._family ~= nil)
		end
	end

	assert(ui.is_id(family), "Expected '*' or ID string for ?family()")
	return function(elem)
		return result(elem._family == family)
	end
end

local function parse_term(str, pred)
	str = str:trim()
	assert(str ~= "", "Expected selector term")

	-- We need to test the first character to see what sort of term we're
	-- dealing with, and then usually parse from the rest of the string.
	local prefix = str:sub(1, 1)
	local suffix = str:sub(2)

	if prefix == "*" then
		-- Universal terms match everything and have no extra stuff to parse.
		return ui._universal_sel, suffix, nil

	elseif prefix == "#" then
		-- Most selectors are similar to the ID selector, in that characters
		-- for the ID string are parsed, and all the characters directly
		-- afterwards are returned as the rest of the string after the term.
		local id, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(id, "Expected ID after '#'")

		return function(elem)
			return result(elem._id == id)
		end, rest, nil

	elseif prefix == "%" then
		local group, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(group, "Expected group after '%'")

		return function(elem)
			return result(elem._groups[group] ~= nil)
		end, rest, nil

	elseif prefix == "@" then
		-- It's possible to check if a box exists in a predicate, but that
		-- leads to different behaviors inside and outside of predicates. For
		-- instance, @main@thumb effectively matches nothing by returning an
		-- empty table of boxes, but would return true for scrollbars if used
		-- in a predicate. So, prevent box selectors in predicates entirely.
		assert(not pred, "Box selectors are invalid in predicate selectors")

		-- First, check if this can be parsed as a universal box selector.
		local name = suffix:sub(1, 1)
		local rest

		if name == "*" then
			rest = suffix:sub(2)

			return function(elem)
				-- If we want all boxes, iterate over the boxes in the element
				-- and add each of them to the full list of boxes.
				local boxes = {}

				for name in pairs(elem._boxes) do
					local box = make_box(name, ui._STATE_NONE)
					boxes[hash_box(box)] = box
				end

				return true, boxes
			end, rest, nil
		end

		-- Otherwise, parse it as a normal box selector instead.
		name, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(name, "Expected box or '*' after '@'")

		return function(elem)
			-- If the box is in the element, return it. Otherwise, the
			-- selector doesn't match.
			if elem._boxes[name] then
				return result(true, name, ui._STATE_NONE)
			end
			return result(false)
		end, rest, nil

	elseif prefix == "$" then
		-- Unfortunately, we can't detect the state of boxes from the server,
		-- so we can't use them in predicates.
		assert(not pred, "State selectors are invalid in predicate selectors")

		local name, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(name, "Expected state after '$'")

		local state = states_by_name[name]
		assert(state, "Invalid state: '" .. name .. "'")

		return function(elem)
			-- States unconditionally match every element. Specify the state
			-- that this term indicates but leave the box undefined.
			return result(true, nil, state)
		end, rest, nil

	elseif prefix == "/" then
		local type, rest = suffix:match("^([" .. ui._ID_CHARS .. "]+)%/(.*)$")
		assert(type, "Expected window type after '/'")

		assert(ui._window_types[type], "Invalid window type: '" .. type .. "'")

		return function(elem)
			return result(elem._window._type == type)
		end, rest, nil

	elseif prefix == "," or prefix == ";" then
		-- Return nil instead of a function and return the prefix character to
		-- instruct ui._parse_sel() to union or end the selector accordingly.
		return nil, suffix, prefix

	elseif prefix == "(" then
		-- Parse a matching set of parentheses, and recursively pass the
		-- contents into ui._parse_sel().
		local sub, rest = str:match("^(%b())(.*)$")
		assert(sub, "Unmatched ')' for '('")

		return ui._parse_sel(sub:sub(2, -2), pred, false), rest, nil

	elseif prefix == "!" then
		-- Parse a single predicate term (NOT an entire predicate selector) and
		-- ensure that it's a valid selector term, not a comma or semicolon.
		local term, rest, _ = parse_term(suffix, true)
		assert(term, "Expected selector term after '!'")

		return function(elem)
			return result(not term(elem))
		end, rest, nil

	elseif prefix == "?" then
		-- Predicates may have different syntax depending on the name of the
		-- predicate, so just parse the name initially.
		local name, after = suffix:match("^([" .. ui._ID_CHARS .. "%<%>]+)(.*)$")
		assert(name, "Expected predicate after '?'")

		-- If this is a simple predicate, return its predicate function without
		-- doing any further parsing.
		local func = simple_preds[name]
		if func then
			return func, after, nil
		end

		-- If this is a function predicate, we need to do more parsing.
		func = func_preds[name]
		if func then
			-- Parse a matching pair of parentheses and get the trimmed
			-- contents between them.
			assert(after:sub(1, 1) == "(", "Expected '(' after '?" .. name .. "'")

			local sub, rest = after:match("^(%b())(.*)$")
			assert(sub, "Unmatched ')' for '?" .. name .. "('")

			local contents = sub:sub(2, -2):trim()
			return func(contents), rest, nil
		end

		-- Otherwise, there is no predicate by this name.
		error("Invalid predicate: '?" .. name .. "'")

	else
		-- If we found no special character, it's either a type or it indicates
		-- invalid characters in the selector string.
		local type, rest = str:match("^([" .. ui._ID_CHARS .. "]+)(.*)$")
		assert(type, "Unexpected character '" .. prefix .. "' in selector")

		assert(ui._elem_types[type], "Invalid element type: '" .. type .. "'")

		return function(elem)
			return result(elem._type == type)
		end, rest, nil
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

function ui._parse_sel(str, pred, partial)
	str = str:trim()
	assert(str ~= "", "Empty style selector")

	local sub_sels = {}
	local terms = {}
	local done = false

	-- Loop until we've read every term from the input string.
	while not done do
		-- Parse the next term from the input string.
		local term, prefix
		term, str, prefix = parse_term(str, pred)

		-- If we read a term, insert this term into the list of terms for the
		-- current sub-selector.
		if term then
			table.insert(terms, term)
		end

		-- Make sure that we have at least one selector term before each comma
		-- or semicolon that we read.
		if prefix then
			assert(#terms > 0, "Expected selector term before '" .. prefix .. "'")
		end

		-- If we read a comma or semicolon or have run out of terms, we need to
		-- commit the terms we've read so far.
		if prefix or str == "" then
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

		-- If we read a semicolon or have run out of terms, we're done parsing.
		-- We check for the semicolon case first since it is possible for the
		-- string to be empty after reading the semicolon.
		if prefix == ";" then
			assert(partial, "Unexpected character ';' in selector")
			done = true
		elseif str == "" then
			assert(prefix ~= ",", "Expected selector term after ','")
			assert(not partial, "Expected ';' after end of selector")
			done = true
		end
	end

	-- Now that we've read all the sub-selectors between the commas, we need to
	-- commit them. We only need to union the terms if there's more than one.
	if #sub_sels == 1 then
		return sub_sels[1], str:trim()
	end
	return ui._union_sels(sub_sels), str:trim()
end
