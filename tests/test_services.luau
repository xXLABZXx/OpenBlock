-- Tier 0/1: services.
compat.check("game global", function()
	assert(game ~= nil)
	assert(game.ClassName == "DataModel")
end)

compat.check("workspace global", function()
	assert(workspace ~= nil)
	assert(workspace.Name == "Workspace")
	assert(workspace == game.Workspace)
end)

compat.check("GetService", function()
	local rs = game:GetService("RunService")
	assert(rs.Name == "RunService")
	assert(game:GetService("Workspace") == workspace)
end)

compat.check("GetService invalid errors", function()
	local ok = pcall(function() game:GetService("Nope") end)
	assert(not ok)
end)

compat.check("RunService.Heartbeat fires", function()
	local n = 0
	game:GetService("RunService").Heartbeat:Connect(function(dt)
		n += 1
		assert(dt > 0)
	end)
	task.wait(0.2)
	assert(n >= 10, "heartbeat fired " .. n .. " times")
end)

compat.check("RunService.IsServer", function()
	assert(game:GetService("RunService"):IsServer() == true)
end)

compat.unsupported("TweenService", "Tier 1, not in v0.1")
compat.unsupported("Debris", "Tier 1, not in v0.1")
compat.unsupported("Players.PlayerAdded", "Tier 1, not in v0.1")
compat.unsupported("DataStoreService", "Tier 2, not in v0.1")
