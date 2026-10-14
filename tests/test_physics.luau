-- Tier 2: physics (gravity integration, AABB collision).
-- OpenBlock's per-frame physics step integrates unanchored parts and resolves
-- collisions against anchored parts.

compat.check("Unanchored part falls", function()
	local ground = Instance.new("Part")
	ground.Size = Vector3.new(20, 1, 20)
	ground.Position = Vector3.new(0, 0, 0)
	ground.Anchored = true
	ground.Parent = workspace
	local p = Instance.new("Part")
	p.Size = Vector3.new(2, 2, 2)
	p.Position = Vector3.new(0, 10, 0)
	p.Parent = workspace
	task.wait(2)
	local y = p.Position.y
	assert(y < 10, "part did not fall, y=" .. y)
	assert(y > 0.5, "part fell through the ground, y=" .. y)
end)

compat.check("Part lands on ground", function()
	local ground = Instance.new("Part")
	ground.Size = Vector3.new(20, 1, 20)
	ground.Position = Vector3.new(40, 0, 0)
	ground.Anchored = true
	ground.Parent = workspace
	local p = Instance.new("Part")
	p.Size = Vector3.new(2, 2, 2)
	p.Position = Vector3.new(40, 10, 0)
	p.Parent = workspace
	task.wait(3)
	local bottom = p.Position.y - p.Size.y / 2
	assert(math.abs(bottom - 0.5) < 0.6, "part did not rest on the ground, bottom=" .. bottom)
end)

compat.check("Anchored part does not fall", function()
	local p = Instance.new("Part")
	p.Size = Vector3.new(2, 2, 2)
	p.Position = Vector3.new(80, 10, 0)
	p.Anchored = true
	p.Parent = workspace
	task.wait(1)
	assert(p.Position.y == 10, "anchored part moved")
end)

compat.check("Velocity integrates", function()
	local p = Instance.new("Part")
	p.Size = Vector3.new(2, 2, 2)
	p.Position = Vector3.new(120, 50, 0)
	p.CanCollide = false
	p.Velocity = Vector3.new(0, 0, 10)
	p.Parent = workspace
	task.wait(0.5)
	assert(p.Position.z > 2, "velocity did not move the part, z=" .. p.Position.z)
end)

compat.check("CanCollide=false passes through", function()
	local ground = Instance.new("Part")
	ground.Size = Vector3.new(20, 1, 20)
	ground.Position = Vector3.new(160, 0, 0)
	ground.Anchored = true
	ground.Parent = workspace
	local p = Instance.new("Part")
	p.Size = Vector3.new(2, 2, 2)
	p.Position = Vector3.new(160, 10, 0)
	p.CanCollide = false
	p.Parent = workspace
	task.wait(2)
	assert(p.Position.y < 0, "part did not pass through, y=" .. p.Position.y)
end)
