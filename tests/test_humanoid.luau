-- Tier 2: Humanoid (health, damage, death, rig building, movement, jumping).
-- OpenBlock's Humanoid is the character controller for avatar rigs.

compat.check("Humanoid TakeDamage and Died", function()
	local model = Instance.new("Model")
	model.Parent = workspace
	local hum = Instance.new("Humanoid")
	hum.Parent = model
	local died = false
	hum.Died:Connect(function() died = true end)
	hum:TakeDamage(30)
	assert(hum.Health == 70, "Health should be 70, got " .. hum.Health)
	hum:TakeDamage(100)
	assert(hum.Health == 0, "Health should clamp at 0, got " .. hum.Health)
	task.wait(0.2)
	assert(died, "Died did not fire")
end)

compat.check("Humanoid health clamps", function()
	local hum = Instance.new("Humanoid")
	hum.Health = 1000
	assert(hum.Health == 100, "Health should clamp to MaxHealth, got " .. hum.Health)
	hum.Health = -5
	assert(hum.Health == 0, "Health should clamp at 0, got " .. hum.Health)
end)

compat.check("Humanoid BuildRig", function()
	local model = Instance.new("Model")
	model.Parent = workspace
	local hum = Instance.new("Humanoid")
	hum.Parent = model
	hum:BuildRig()
	local root = model:FindFirstChild("HumanoidRootPart")
	assert(root ~= nil, "HumanoidRootPart missing")
	assert(root:IsA("Part"), "HumanoidRootPart is not a Part")
	local n = 0
	for _, c in ipairs(model:GetChildren()) do
		if c:IsA("Part") then n = n + 1 end
	end
	assert(n == 6, "expected 6 rig parts, got " .. n)
end)

compat.check("Humanoid MoveTo walks", function()
	local model = Instance.new("Model")
	model.Parent = workspace
	local hum = Instance.new("Humanoid")
	hum.Parent = model
	local ground = Instance.new("Part")
	ground.Size = Vector3.new(40, 1, 20)
	ground.Position = Vector3.new(15, 0, 0)
	ground.Anchored = true
	ground.Parent = workspace
	hum:BuildRig()
	hum:MoveTo(Vector3.new(20, 4, 0))
	task.wait(3)
	local root = model:FindFirstChild("HumanoidRootPart")
	assert(root.Position.x > 5, "root did not walk toward the target, x=" .. root.Position.x)
end)

compat.check("Humanoid Jump", function()
	local model = Instance.new("Model")
	model.Parent = workspace
	local hum = Instance.new("Humanoid")
	hum.Parent = model
	local ground = Instance.new("Part")
	ground.Size = Vector3.new(20, 1, 20)
	ground.Position = Vector3.new(0, 0, 0)
	ground.Anchored = true
	ground.Parent = workspace
	hum:BuildRig()
	task.wait(1) -- let the rig settle onto the ground
	local root = model:FindFirstChild("HumanoidRootPart")
	local restY = root.Position.y
	hum.Jump = true
	local peak = restY
	for i = 1, 36 do
		task.wait(1 / 60)
		local y = root.Position.y
		if y > peak then peak = y end
	end
	assert(peak > restY + 2, "jump did not lift the root, peak=" .. peak .. " rest=" .. restY)
end)
