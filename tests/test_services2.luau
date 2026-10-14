-- Tier 1: Lighting, Teams, SoundService, Debris, BindableEvent,
-- BindableFunction, Camera and Workspace extras.

compat.check("Lighting properties round-trip", function()
	local l = game:GetService("Lighting")
	assert(l.Brightness == 2)
	assert(l.ClockTime == 14)
	assert(l.TimeOfDay == "14:00:00")
	assert(l.FogEnd == 100000)
	l.ClockTime = 6.5
	assert(math.abs(l.ClockTime - 6.5) < 1e-9)
	l.FogEnd = 500
	assert(l.FogEnd == 500)
	l.Ambient = Color3.new(0.1, 0.2, 0.3)
	local a = l.Ambient
	assert(math.abs(a.r - 0.1) < 1e-9 and math.abs(a.g - 0.2) < 1e-9 and math.abs(a.b - 0.3) < 1e-9)
	-- restore defaults for other tests
	l.ClockTime = 14
	l.FogEnd = 100000
	l.Ambient = Color3.new(0.5, 0.5, 0.5)
end)

compat.check("Teams GetTeams lists teams", function()
	local teams = game:GetService("Teams")
	local t = Instance.new("Team")
	t.Name = "Red"
	t.AutoAssignable = false
	t.Parent = teams
	local list = teams:GetTeams()
	assert(#list == 1 and list[1] == t)
	assert(t.AutoAssignable == false)
	t.TeamColor = Color3.new(1, 0, 0)
	assert(t.TeamColor.r == 1)
	t:Destroy()
	assert(#teams:GetTeams() == 0)
end)

compat.check("Sound Play and Stop toggle Playing", function()
	local s = Instance.new("Sound")
	assert(s.Playing == false)
	assert(s.Volume == 0.5)
	s:Play()
	assert(s.Playing == true)
	s:Pause()
	assert(s.Playing == false)
	s:Resume()
	assert(s.Playing == true)
	s:Stop()
	assert(s.Playing == false)
end)

compat.check("Debris destroys item after lifetime", function()
	local part = Instance.new("Part")
	part.Name = "DebrisVictim"
	part.Parent = workspace
	assert(workspace:FindFirstChild("DebrisVictim") == part)
	game:GetService("Debris"):AddItem(part, 0.2)
	task.wait(0.1)
	assert(workspace:FindFirstChild("DebrisVictim") == part, "destroyed too early")
	task.wait(0.5)
	assert(workspace:FindFirstChild("DebrisVictim") == nil, "Debris did not destroy the part")
end)

compat.check("BindableEvent Fire passes args", function()
	local be = Instance.new("BindableEvent")
	local gotA, gotB = nil, nil
	be.Event:Connect(function(a, b)
		gotA, gotB = a, b
	end)
	be:Fire("hello", 42)
	task.wait(0.2)
	assert(gotA == "hello" and gotB == 42, "handler got wrong args")
end)

compat.check("BindableFunction Invoke calls bound function", function()
	local bf = Instance.new("BindableFunction")
	bf:SetOnInvoke(function(a, b)
		return a + b
	end)
	assert(bf:Invoke(20, 22) == 42)
	bf:SetOnInvoke(function()
		return 1, 2, 3
	end)
	local x, y, z = bf:Invoke()
	assert(x == 1 and y == 2 and z == 3, "multiple return values broken")
	local ok = pcall(function()
		local bf2 = Instance.new("BindableFunction")
		bf2:Invoke()
	end)
	assert(not ok, "Invoke without SetOnInvoke did not error")
end)

compat.check("Camera default CFrame", function()
	local cam = workspace.CurrentCamera
	assert(cam ~= nil and cam:IsA("Camera"))
	assert(cam.FieldOfView == 70)
	assert(cam.CameraType == "Custom")
	local p = cam.CFrame.Position
	assert(math.abs(p.x) < 1e-9 and math.abs(p.y - 12) < 1e-9 and math.abs(p.z - 24) < 1e-9)
end)

compat.check("Workspace CurrentCamera and Gravity", function()
	assert(workspace.CurrentCamera:IsA("Camera"))
	assert(workspace.Gravity == 196.2)
	workspace.Gravity = 100
	assert(workspace.Gravity == 100)
	workspace.Gravity = 196.2
end)
