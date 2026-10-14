-- Tier 0: Vector3 datatype.
compat.check("Vector3.new", function()
	local v = Vector3.new(1, 2, 3)
	assert(v.x == 1 and v.y == 2 and v.z == 3)
end)

compat.check("Vector3 arithmetic", function()
	local a = Vector3.new(1, 2, 3)
	local b = Vector3.new(4, 5, 6)
	local c = a + b
	assert(c.x == 5 and c.y == 7 and c.z == 9)
	local d = b - a
	assert(d.x == 3 and d.y == 3 and d.z == 3)
	local e = a * 2
	assert(e.x == 2 and e.y == 4 and e.z == 6)
	local f = 2 * a
	assert(f.x == 2 and f.y == 4 and f.z == 6)
end)

compat.check("Vector3 equality", function()
	assert(Vector3.new(1, 1, 1) == Vector3.new(1, 1, 1))
	assert(Vector3.new(1, 1, 1) ~= Vector3.new(1, 1, 2))
end)

compat.check("Vector3 Magnitude", function()
	assert(Vector3.new(3, 4, 0).Magnitude == 5)
end)

compat.check("Vector3 invalid member errors", function()
	local ok = pcall(function() return Vector3.new().nope end)
	assert(not ok)
end)

compat.partial("Vector3", "Lerp, Cross, Dot, unit etc. not in v0.1")
compat.unsupported("CFrame", "not implemented in v0.1")
compat.unsupported("UDim/UDim2/Enum", "not implemented in v0.1")
