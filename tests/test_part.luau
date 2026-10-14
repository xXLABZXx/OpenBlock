-- Tier 1: Part.
compat.check("Part properties", function()
	local p = Instance.new("Part")
	p.Position = Vector3.new(1, 2, 3)
	local v = p.Position
	assert(v.x == 1 and v.y == 2 and v.z == 3)
	p.Size = Vector3.new(4, 5, 6)
	assert(p.Size.y == 5)
	p.Anchored = true
	assert(p.Anchored == true)
	p.Transparency = 0.5
	assert(p.Transparency == 0.5)
	p.Color = Color3.fromRGB(255, 0, 0)
	local c = p.Color
	assert(c.r == 1 and c.g == 0 and c.b == 0)
end)

compat.check("Color3.new", function()
	local c = Color3.new(0.5, 0.25, 1)
	assert(c.r == 0.5 and c.g == 0.25 and c.b == 1)
	assert(Color3.fromRGB(255, 128, 0) == Color3.new(1, 128 / 255, 0))
end)

compat.check("Part property type mismatch errors", function()
	local p = Instance.new("Part")
	local ok = pcall(function() p.Position = "nope" end)
	assert(not ok)
end)

compat.check("Part read-only ClassName", function()
	local p = Instance.new("Part")
	local ok = pcall(function() p.ClassName = "Model" end)
	assert(not ok)
end)

compat.check("Part.Touched fires on overlap", function()
	local a = Instance.new("Part")
	a.Position = Vector3.new(0, 0, 0)
	a.Size = Vector3.new(2, 2, 2)
	a.Parent = workspace
	local b = Instance.new("Part")
	b.Position = Vector3.new(0, 5, 0)
	b.Size = Vector3.new(2, 2, 2)
	b.Parent = workspace
	local other = nil
	a.Touched:Connect(function(o) other = o end)
	for i = 1, 60 do
		local p = b.Position
		b.Position = Vector3.new(p.x, p.y - 0.2, p.z)
		task.wait(1 / 60)
	end
	assert(other == b, "Touched did not fire")
end)

compat.unsupported("Workspace.Raycast", "Tier 1, not in v0.1")
compat.unsupported("Humanoid", "Tier 1, not in v0.1")
compat.unsupported("RemoteEvent", "Tier 1, not in v0.1")
