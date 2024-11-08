-- This is an implementation of a job sheduling mechanism. It guarantees that
-- coexisting jobs will execute primarily in order of least expiry, and
-- secondarily in order of first registration.

-- These functions implement an intrusive singly linked list of one or more
-- elements where the first element has a pointer to the last. The next pointer
-- is stored with key list_next. The pointer to the last is with key list_end.

local function list_init(first)
	first.list_end = first
end

local function list_append(first, append)
	first.list_end.list_next = append
	first.list_end = append
end

local function list_append_list(first, first_append)
	first.list_end.list_next = first_append
	first.list_end = first_append.list_end
end

-- The jobs are stored in a map from expiration times to linked lists of jobs
-- as above. The expiration times are also stored in an array representing a
-- binary min heap, which is a particular arrangement of binary tree. A parent
-- at index i has children at indices i*2 and i*2+1. Out-of-bounds indices
-- represent nonexistent children. A parent is never greater than its children.
-- This structure means that, if there is at least one job, the next expiration
-- time is the first item in the array.

-- Push element on a binary min-heap,
-- "bubbling up" the element by swapping with larger parents.
local function heap_push(heap, element)
	local index = #heap + 1
	while index > 1 do
		local parent_index = math.floor(index / 2)
		local parent = heap[parent_index]
		if element < parent then
			heap[index] = parent
			index = parent_index
		else
			break
		end
	end
	heap[index] = element
end

-- Pop smallest element from the heap,
-- "sinking down" the last leaf on the last layer of the heap
-- by swapping with the smaller child.
local function heap_pop(heap)
	local removed_element = heap[1]
	local length = #heap
	local element = heap[length]
	heap[length] = nil
	length = length - 1
	if length > 0 then
		local index = 1
		while true do
			local old_index = index
			local smaller_element = element
			local left_index = index * 2
			local right_index = index * 2 + 1
			if left_index <= length then
				local left_element = heap[left_index]
				if left_element < smaller_element then
					index = left_index
					smaller_element = left_element
				end
			end
			if right_index <= length then
				if heap[right_index] < smaller_element then
					index = right_index
				end
			end
			if old_index ~= index then
				heap[old_index] = heap[index]
			else
				break
			end
		end
		heap[index] = element
	end
	return removed_element
end

local job_map = {}
local expiries = {}

-- Adds an individual job with the given expiry.
-- The worst-case complexity is O(log n), where n is the number of distinct
-- expiration times.
local function add_job(expiry, job)
	local list = job_map[expiry]
	if list then
		list_append(list, job)
	else
		list_init(job)
		job_map[expiry] = job
		heap_push(expiries, expiry)
	end
end

-- Removes the next expiring jobs and returns the linked list of them.
-- The worst-case complexity is O(log n), where n is the number of distinct
-- expiration times.
local function remove_first_jobs()
	local removed_expiry = heap_pop(expiries)
	local removed = job_map[removed_expiry]
	job_map[removed_expiry] = nil
	return removed
end

local time = 0.0
local time_next = math.huge

core.register_globalstep(function(dtime)
	time = time + dtime

	if time < time_next then
		return
	end

	-- Remove the expired jobs.
	local expired = remove_first_jobs()

	-- Remove other expired jobs and append them to the list.
	while true do
		time_next = expiries[1] or math.huge
		if time_next > time then
			break
		end
		list_append_list(expired, remove_first_jobs())
	end

	-- Run the callbacks afterward to prevent infinite loops with core.after(0, ...).
	local last_expired = expired.list_end
	while true do
		core.set_last_run_mod(expired.mod_origin)
		expired.func(unpack(expired.args, 1, expired.args.n))
		if expired == last_expired then
			break
		end
		expired = expired.list_next
	end
end)

local job_metatable = {__index = {}}

local function dummy_func() end
function job_metatable.__index:cancel()
	self.func = dummy_func
	self.args = {n = 0}
end

function core.after(after, func, ...)
	assert(tonumber(after) and not core.is_nan(after) and type(func) == "function",
		"Invalid core.after invocation")

	local new_job = {
		mod_origin = core.get_last_run_mod(),
		func = func,
		args = {
			n = select("#", ...),
			...
		},
	}

	local expiry = time + after
	add_job(expiry, new_job)
	time_next = math.min(time_next, expiry)

	return setmetatable(new_job, job_metatable)
end
