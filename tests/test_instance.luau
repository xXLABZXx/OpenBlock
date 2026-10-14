-- Tier 0: Instance core.
compat.check("Instance.new Part", function()
	local p = Instance.new("Part")
	assert(p.ClassName == "Part")
	assert(p.Name == "Part")
end)

compat.check("Instance.new invalid class errors", function()
	local ok = pcall(function() Instance.new("Nope") end)
	assert(not ok, "expected an error")
end)

compat.check("Instance parenting", function()
	local m = Instance.new("Model")
	local p = Instance.new("Part")
	p.Parent = m
	assert(p.Parent == m)
	assert(#m:GetChildren() == 1)
	assert(m:GetChildren()[1] == p)
end)

compat.check("Instance.new with parent arg", function()
	local m = Instance.new("Model")
	local p = Instance.new("Part", m)
	assert(p.Parent == m)
end)

compat.check("Instance ChildAdded fires", function()
	local m = Instance.new("Model")
	local seen = nil
	m.ChildAdded:Connect(function(c) seen = c end)
	local p = Instance.new("Part", m)
	task.wait(0.1)
	assert(seen == p, "ChildAdded did not fire")
end)

compat.check("Instance ChildRemoved fires", function()
	local m = Instance.new("Model")
	local p = Instance.new("Part", m)
	local seen = nil
	m.ChildRemoved:Connect(function(c) seen = c end)
	p.Parent = nil
	task.wait(0.1)
	assert(seen == p, "ChildRemoved did not fire")
end)

compat.check("Instance FindFirstChild", function()
	local m = Instance.new("Model")
	local p = Instance.new("Part")
	p.Name = "Foo"
	p.Parent = m
	assert(m:FindFirstChild("Foo") == p)
	assert(m:FindFirstChild("Bar") == nil)
end)

compat.check("Instance GetDescendants", function()
	local m = Instance.new("Model")
	local f = Instance.new("Folder", m)
	local p = Instance.new("Part", f)
	assert(#m:GetDescendants() == 2)
end)

compat.check("Instance IsA", function()
	local p = Instance.new("Part")
	assert(p:IsA("Part"))
	assert(p:IsA("BasePart"))
	assert(p:IsA("Instance"))
	assert(not p:IsA("Model"))
end)

compat.check("Instance Changed fires", function()
	local p = Instance.new("Part")
	local key = nil
	p.Changed:Connect(function(k) key = k end)
	p.Name = "Renamed"
	task.wait(0.1)
	assert(key == "Name")
end)

compat.check("Instance Destroy", function()
	local m = Instance.new("Model")
	local p = Instance.new("Part", m)
	p:Destroy()
	assert(#m:GetChildren() == 0)
end)

compat.check("Instance invalid member errors", function()
	local p = Instance.new("Part")
	local ok = pcall(function() return p.Nope end)
	assert(not ok)
end)

compat.check("Instance WaitForChild", function()
	local m = Instance.new("Model")
	task.delay(0.2, function()
		local p = Instance.new("Part")
		p.Name = "Late"
		p.Parent = m
	end)
	local found = m:WaitForChild("Late", 2)
	assert(found ~= nil and found.Name == "Late")
end)

compat.check("Instance.Clone", function()
	local p = Instance.new("Part")
	p.Name = "Original"
	p.Position = Vector3.new(1, 2, 3)
	local c = p:Clone()
	assert(c ~= p)
	assert(c.ClassName == "Part")
	assert(c.Name == "Original")
	assert(c.Position == Vector3.new(1, 2, 3))
	assert(c.Parent == nil)
end)

compat.check("Instance attributes", function()
	local p = Instance.new("Part")
	p:SetAttribute("Score", 100)
	p:SetAttribute("Label", "hi")
	assert(p:GetAttribute("Score") == 100)
	assert(p:GetAttribute("Label") == "hi")
	assert(p:GetAttribute("Missing") == nil)
end)

compat.check("Instance tags", function()
	local p = Instance.new("Part")
	p:AddTag("Enemy")
	assert(p:HasTag("Enemy"))
	assert(not p:HasTag("Friend"))
	p:RemoveTag("Enemy")
	assert(not p:HasTag("Enemy"))
end)

compat.check("Instance GetPropertyChangedSignal", function()
	local p = Instance.new("Part")
	local got = nil
	p:GetPropertyChangedSignal("Name"):Connect(function() got = true end)
	p.Name = "Changed!"
	task.wait(0.1)
	assert(got == true)
end)
