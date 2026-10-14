-- Tier 1: ClickDetector.
compat.check("ClickDetector creation", function()
	local cd = Instance.new("ClickDetector")
	assert(cd.ClassName == "ClickDetector")
	assert(cd:IsA("ClickDetector"))
	assert(cd:IsA("Instance"))
end)

compat.check("ClickDetector MouseClick Connect/Disconnect", function()
	local cd = Instance.new("ClickDetector")
	local c = cd.MouseClick:Connect(function() end)
	assert(c.Connected == true)
	c:Disconnect()
	assert(c.Connected == false)
end)

compat.check("ClickDetector MouseClick signal stable across accesses", function()
	local cd = Instance.new("ClickDetector")
	-- Signal userdata now has __eq on the underlying C++ Signal, so identity
	-- holds across accesses.
	local s1 = cd.MouseClick
	local s2 = cd.MouseClick
	assert(s1 == s2, "MouseClick identity broken across accesses")
	assert(type(s1.Connect) == "function")
	assert(type(s1.Once) == "function" and type(s2.Wait) == "function")
	-- Connections register through either access.
	local c1 = s1:Connect(function() end)
	assert(c1.Connected == true)
	local c2 = s2:Connect(function() end)
	assert(c2.Connected == true)
	c1:Disconnect()
	assert(c1.Connected == false)
	assert(c2.Connected == true)
	c2:Disconnect()
end)

-- End-to-end firing (input driving MouseClick) awaits the input layer.
compat.unsupported("ClickDetector input-driven firing", "headless v0.1 has no input path")
