-- Tier 1: TweenService and TweenInfo.
local TweenService = game:GetService("TweenService")

compat.check("TweenInfo.new validates arguments", function()
	assert(pcall(function() return TweenInfo.new(1) end))
	assert(pcall(function() return TweenInfo.new(1, 2, 1, 0, false, 0.5) end))
	assert(not pcall(function() return TweenInfo.new(-1) end), "negative time accepted")
	assert(not pcall(function() return TweenInfo.new(1, 99) end), "bad easing style accepted")
	assert(not pcall(function() return TweenInfo.new(1, 1, 99) end), "bad easing direction accepted")
	assert(not pcall(function() return TweenInfo.new("x") end), "non-number time accepted")
end)

compat.check("TweenService Create and Play moves Position", function()
	local part = Instance.new("Part")
	part.Position = Vector3.new(0, 0, 0)
	part.Anchored = true -- keep gravity out of this test
	part.Parent = workspace
	local done = false
	local tween = TweenService:Create(part, TweenInfo.new(0.3), {
		Position = Vector3.new(10, 0, 0),
	})
	tween.Completed:Connect(function()
		done = true
	end)
	tween:Play()
	task.wait(0.6)
	local p = part.Position
	assert(math.abs(p.x - 10) < 0.5, "tween did not reach target x=" .. p.x)
	assert(math.abs(p.y) < 0.5 and math.abs(p.z) < 0.5)
	assert(done, "Completed did not fire")
	assert(tween.PlaybackState == "Completed")
end)

compat.check("TweenService tweens Transparency", function()
	local part = Instance.new("Part")
	part.Transparency = 0
	part.Parent = workspace
	local tween = TweenService:Create(part, TweenInfo.new(0.2, 0), {
		Transparency = 1,
	})
	assert(tween.PlaybackState == "Delayed")
	tween:Play()
	assert(tween.PlaybackState == "Playing")
	task.wait(0.4)
	assert(math.abs(part.Transparency - 1) < 1e-6, "Transparency did not reach 1")
	assert(tween.PlaybackState == "Completed")
end)

compat.check("Tween Cancel stops the tween", function()
	local part = Instance.new("Part")
	part.Position = Vector3.new(0, 0, 0)
	part.Parent = workspace
	local done = false
	local tween = TweenService:Create(part, TweenInfo.new(1), {
		Position = Vector3.new(10, 0, 0),
	})
	tween.Completed:Connect(function()
		done = true
	end)
	tween:Play()
	task.wait(0.3)
	tween:Cancel()
	local xAtCancel = part.Position.x
	task.wait(0.8)
	assert(math.abs(part.Position.x - xAtCancel) < 1e-9, "tween moved after Cancel")
	assert(not done, "Completed fired after Cancel")
	assert(tween.PlaybackState == "Cancelled")
end)

compat.check("Tween Pause freezes progress", function()
	local part = Instance.new("Part")
	part.Position = Vector3.new(0, 0, 0)
	part.Parent = workspace
	local tween = TweenService:Create(part, TweenInfo.new(1), {
		Position = Vector3.new(10, 0, 0),
	})
	tween:Play()
	task.wait(0.2)
	tween:Pause()
	assert(tween.PlaybackState == "Paused")
	local xAtPause = part.Position.x
	task.wait(0.4)
	assert(math.abs(part.Position.x - xAtPause) < 1e-9, "tween moved while paused")
end)

compat.check("TweenService Create rejects unknown property", function()
	local part = Instance.new("Part")
	local ok = pcall(function()
		TweenService:Create(part, TweenInfo.new(1), { NoSuchProp = 1 })
	end)
	assert(not ok, "unknown goal property was accepted")
end)
