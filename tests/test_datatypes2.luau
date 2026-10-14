-- Tier 1: UDim, UDim2, BrickColor, Enum datatypes and Part math properties.
local function approx(a, b)
	return math.abs(a - b) < 1e-4
end

compat.check("UDim.new", function()
	local u = UDim.new(0.5, 10)
	assert(u.Scale == 0.5 and u.Offset == 10)
	local s = u + UDim.new(0.25, 5)
	assert(s.Scale == 0.75 and s.Offset == 15)
	local d = s - u
	assert(d.Scale == 0.25 and d.Offset == 5)
	assert(u == UDim.new(0.5, 10))
	assert(u ~= UDim.new(0.5, 11))
end)

compat.check("UDim2.new", function()
	local u = UDim2.new(0.5, 10, 0.25, 20)
	assert(u.XScale == 0.5 and u.XOffset == 10)
	assert(u.YScale == 0.25 and u.YOffset == 20)
	assert(u.Width == 10 and u.Height == 20)
end)

compat.check("UDim2.fromScale", function()
	local u = UDim2.fromScale(0.5, 0.25)
	assert(u.XScale == 0.5 and u.YScale == 0.25)
	assert(u.XOffset == 0 and u.YOffset == 0)
end)

compat.check("UDim2.fromOffset", function()
	local u = UDim2.fromOffset(10, 20)
	assert(u.XScale == 0 and u.YScale == 0)
	assert(u.XOffset == 10 and u.YOffset == 20)
end)

compat.check("UDim2 arithmetic", function()
	local a = UDim2.new(1, 2, 3, 4)
	local b = UDim2.new(5, 6, 7, 8)
	local c = a + b
	assert(c.XScale == 6 and c.XOffset == 8 and c.YScale == 10 and c.YOffset == 12)
	local d = b - a
	assert(d.XScale == 4 and d.XOffset == 4 and d.YScale == 4 and d.YOffset == 4)
	assert(a == UDim2.new(1, 2, 3, 4))
	assert(a ~= b)
end)

compat.check("UDim2 invalid member errors", function()
	local ok = pcall(function() return UDim2.new(1, 2, 3, 4).nope end)
	assert(not ok)
end)

compat.check("BrickColor.new name", function()
	local bc = BrickColor.new("Bright red")
	assert(bc.Name == "Bright red")
	assert(bc.Number == 1)
	assert(bc.Color.r > 0.5)
	-- Name lookup is case-insensitive.
	assert(BrickColor.new("bright red").Number == 1)
end)

compat.check("BrickColor.new number", function()
	local bc = BrickColor.new(1)
	assert(bc.Name == "Bright red" and bc.Number == 1)
	assert(BrickColor.new(bc.Number) == bc)
end)

compat.check("BrickColor invalid name errors", function()
	local ok = pcall(function() return BrickColor.new("Not a real color") end)
	assert(not ok)
	local ok2 = pcall(function() return BrickColor.new(99999) end)
	assert(not ok2)
end)

compat.check("Enum values", function()
	assert(Enum.Material.Plastic.Value == 0)
	assert(Enum.Material.Ice.Value == 15)
	assert(Enum.EasingStyle.Linear.Value == 0)
	assert(Enum.EasingStyle.Elastic.Value == 10)
	assert(Enum.EasingDirection.In.Value == 0)
	assert(Enum.EasingDirection.Out.Value == 1)
	assert(Enum.EasingDirection.InOut.Value == 2)
	assert(Enum.HumanoidStateType.Running.Value == 0)
	assert(Enum.SortOrder.Custom.Value == 1)
	assert(Enum.Font.Gotham.Value == 2)
	assert(Enum.PlaybackState.Playing.Value == 0)
	assert(tostring(Enum.Material.Plastic) == "Enum.Material.Plastic")
	assert(Enum.Material.Plastic == Enum.Material.Plastic)
	assert(Enum.Material.Plastic ~= Enum.Material.Neon)
	local ok = pcall(function() return Enum.Material.Nope end)
	assert(not ok)
end)

compat.check("Part CFrame sync", function()
	local part = Instance.new("Part")
	part.Position = Vector3.new(1, 2, 3)
	local cp = part.CFrame.Position
	assert(approx(cp.x, 1) and approx(cp.y, 2) and approx(cp.z, 3))
	part.CFrame = CFrame.new(4, 5, 6)
	local pp = part.Position
	assert(approx(pp.x, 4) and approx(pp.y, 5) and approx(pp.z, 6))
	-- A Position write keeps the existing CFrame rotation.
	part.CFrame = CFrame.Angles(0, math.rad(90), 0)
	part.Position = Vector3.new(9, 9, 9)
	local rv = part.CFrame.RightVector
	assert(approx(rv.x, 0) and approx(rv.y, 0) and approx(rv.z, -1))
	local pp2 = part.Position
	assert(approx(pp2.x, 9) and approx(pp2.y, 9) and approx(pp2.z, 9))
end)

compat.check("Part math property defaults", function()
	local part = Instance.new("Part")
	assert(part.Material == "Plastic")
	assert(part.Shape == "Block")
	local v = part.Velocity
	assert(v.x == 0 and v.y == 0 and v.z == 0)
	part.Material = "Wood"
	assert(part.Material == "Wood")
	part.Velocity = Vector3.new(0, 5, 0)
	assert(part.Velocity.y == 5)
end)
