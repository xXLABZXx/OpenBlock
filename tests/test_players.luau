-- Tier 1: Players service (PlayerAdded/PlayerRemoving, GetPlayers).
local Players = game:GetService("Players")

compat.check("Players tracks PlayerAdded", function()
	local seen = {}
	local conn = Players.PlayerAdded:Connect(function(p)
		table.insert(seen, p)
	end)
	local p = Instance.new("Player")
	p.Name = "Alice"
	p.UserId = 123
	p.DisplayName = "Alice"
	p.Parent = Players
	task.wait(0.3)
	local found = false
	for _, q in seen do
		if q == p then
			found = true
		end
	end
	assert(found, "PlayerAdded did not fire for the new player")
	assert(p.Name == "Alice")
	conn:Disconnect()
	p:Destroy()
end)

compat.check("Players GetPlayers lists current players", function()
	-- Identity-based (not count-based): checks in this file run concurrently
	-- and share the Players service, so absolute counts race.
	local p = Instance.new("Player")
	p.Parent = Players
	task.wait(0.3)
	local found = false
	for _, q in Players:GetPlayers() do
		if q == p then
			found = true
		end
	end
	assert(found, "new player not listed by GetPlayers")
	p:Destroy()
	task.wait(0.3)
	for _, q in Players:GetPlayers() do
		assert(q ~= p, "destroyed player still listed")
	end
end)

compat.check("Players GetPlayerByUserId finds player", function()
	local p = Instance.new("Player")
	p.UserId = 424242
	p.Parent = Players
	task.wait(0.3)
	assert(Players:GetPlayerByUserId(424242) == p)
	assert(Players:GetPlayerByUserId(999999) == nil)
	p:Destroy()
end)

compat.check("Players fires PlayerRemoving on Destroy", function()
	local gone = {}
	local conn = Players.PlayerRemoving:Connect(function(p)
		table.insert(gone, p)
	end)
	local p = Instance.new("Player")
	p.Parent = Players
	task.wait(0.3)
	p:Destroy()
	task.wait(0.3)
	local found = false
	for _, q in gone do
		if q == p then
			found = true
		end
	end
	assert(found, "PlayerRemoving did not fire for the destroyed player")
	conn:Disconnect()
end)

compat.check("Player Character assign", function()
	local p = Instance.new("Player")
	assert(p.Character == nil)
	assert(p.DisplayName == "")
	local m = Instance.new("Model")
	p.Character = m
	assert(p.Character == m, "Character did not round-trip")
end)
