-- Tier 0: RBXScriptSignal.
compat.check("Signal Connect+Fire via Changed", function()
	local p = Instance.new("Part")
	local got = nil
	p.Changed:Connect(function(k) got = k end)
	p.Name = "X"
	task.wait(0.1)
	assert(got == "Name")
end)

compat.check("Signal Disconnect", function()
	local p = Instance.new("Part")
	local n = 0
	local c = p.Changed:Connect(function() n += 1 end)
	c:Disconnect()
	p.Name = "Y"
	task.wait(0.1)
	assert(n == 0)
end)

compat.check("Signal Once", function()
	local p = Instance.new("Part")
	local n = 0
	p.Changed:Once(function() n += 1 end)
	p.Name = "A"
	p.Name = "B"
	task.wait(0.2)
	assert(n == 1, "Once fired " .. n .. " times")
end)

compat.check("Signal Wait", function()
	local p = Instance.new("Part")
	local got = nil
	task.spawn(function()
		got = p.Changed:Wait()
	end)
	task.wait(0.05)
	p.Name = "Z"
	task.wait(0.1)
	assert(got == "Name")
end)

compat.check("Connection.Connected", function()
	local p = Instance.new("Part")
	local c = p.Changed:Connect(function() end)
	assert(c.Connected == true)
	c:Disconnect()
	assert(c.Connected == false)
end)
