-- Tier 1: CFrame datatype.
local function approx(a, b)
	return math.abs(a - b) < 1e-4
end

local function vecApprox(v, x, y, z)
	return approx(v.x, x) and approx(v.y, y) and approx(v.z, z)
end

compat.check("CFrame.new defaults", function()
	local cf = CFrame.new()
	assert(vecApprox(cf.Position, 0, 0, 0))
	assert(cf.X == 0 and cf.Y == 0 and cf.Z == 0)
	assert(vecApprox(cf.RightVector, 1, 0, 0))
	assert(vecApprox(cf.UpVector, 0, 1, 0))
	assert(vecApprox(cf.LookVector, 0, 0, -1))
end)

compat.check("CFrame.new position", function()
	local cf = CFrame.new(1, 2, 3)
	assert(cf.X == 1 and cf.Y == 2 and cf.Z == 3)
	assert(vecApprox(cf.Position, 1, 2, 3))
	local cf2 = CFrame.new(Vector3.new(4, 5, 6))
	assert(vecApprox(cf2.Position, 4, 5, 6))
end)

compat.check("CFrame.new lookAt", function()
	local cf = CFrame.new(Vector3.new(0, 0, 0), Vector3.new(0, 0, -1))
	assert(vecApprox(cf.LookVector, 0, 0, -1))
	assert(vecApprox(cf.Position, 0, 0, 0))
end)

compat.check("CFrame Angles rotation", function()
	local cf = CFrame.Angles(0, math.rad(90), 0)
	-- Y-90 rotation maps unit X to -Z.
	assert(vecApprox(cf.RightVector, 0, 0, -1))
	assert(vecApprox(cf:VectorToWorldSpace(Vector3.new(1, 0, 0)), 0, 0, -1))
	local cf2 = CFrame.fromEulerAnglesXYZ(0, math.rad(90), 0)
	assert(vecApprox(cf2.RightVector, 0, 0, -1))
end)

compat.check("CFrame multiplication", function()
	local a = CFrame.new(1, 0, 0)
	local b = CFrame.new(0, 2, 0)
	local c = a * b
	assert(vecApprox(c.Position, 1, 2, 0))
	-- Frame times point applies the full transform to the point.
	local p = CFrame.new(1, 2, 3) * Vector3.new(0, 0, 0)
	assert(vecApprox(p, 1, 2, 3))
end)

compat.check("CFrame inverse", function()
	local cf = CFrame.new(3, 4, 5) * CFrame.Angles(0, math.rad(45), 0)
	local id = cf * cf:Inverse()
	local comps = { id:GetComponents() }
	local want = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 }
	for i = 1, 12 do
		assert(approx(comps[i], want[i]))
	end
end)

compat.check("CFrame lerp", function()
	local a = CFrame.new(0, 0, 0)
	local b = CFrame.new(10, 0, 0)
	local m = a:Lerp(b, 0.5)
	assert(vecApprox(m.Position, 5, 0, 0))
end)

compat.check("CFrame GetComponents", function()
	local cf = CFrame.new(1, 2, 3)
	local comps = { cf:GetComponents() }
	assert(#comps == 12)
	assert(comps[1] == 1 and comps[5] == 1 and comps[9] == 1)
	assert(comps[10] == 1 and comps[11] == 2 and comps[12] == 3)
end)

compat.check("CFrame space transforms", function()
	local cf = CFrame.new(1, 0, 0)
	assert(vecApprox(cf:PointToWorldSpace(Vector3.new(1, 1, 1)), 2, 1, 1))
	assert(vecApprox(cf:VectorToWorldSpace(Vector3.new(1, 1, 1)), 1, 1, 1))
	assert(vecApprox(cf:PointToObjectSpace(Vector3.new(2, 1, 1)), 1, 1, 1))
	assert(vecApprox(cf:VectorToObjectSpace(Vector3.new(1, 1, 1)), 1, 1, 1))
	local a = CFrame.new(1, 0, 0)
	local b = CFrame.new(0, 2, 0)
	assert(vecApprox(a:ToWorldSpace(b).Position, 1, 2, 0))
	assert(vecApprox(a:ToObjectSpace(b).Position, -1, 2, 0))
end)

compat.check("CFrame add and sub", function()
	local cf = CFrame.new(1, 2, 3)
	local v = Vector3.new(10, 0, 0)
	assert(vecApprox((cf + v).Position, 11, 2, 3))
	assert(vecApprox((cf - v).Position, -9, 2, 3))
	local d = CFrame.new(1, 2, 3) - CFrame.new(4, 6, 8)
	assert(vecApprox(d, -3, -4, -5))
end)

compat.check("CFrame equality", function()
	assert(CFrame.new(1, 2, 3) == CFrame.new(1, 2, 3))
	assert(CFrame.new(1, 2, 3) ~= CFrame.new(1, 2, 4))
end)

compat.check("CFrame tostring", function()
	local s = tostring(CFrame.new(1, 2, 3))
	assert(string.find(s, "1, 2, 3", 1, true) ~= nil)
end)

compat.check("CFrame invalid member errors", function()
	local ok = pcall(function() return CFrame.new().nope end)
	assert(not ok)
end)
