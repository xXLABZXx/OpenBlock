-- Tier 1: Value classes (IntValue, NumberValue, StringValue, BoolValue).
compat.check("IntValue default and round-trip", function()
	local v = Instance.new("IntValue")
	assert(v.ClassName == "IntValue")
	assert(v.Value == 0)
	v.Value = 42
	assert(v.Value == 42)
end)

compat.check("NumberValue default and round-trip", function()
	local v = Instance.new("NumberValue")
	assert(v.Value == 0)
	v.Value = 3.5
	assert(v.Value == 3.5)
end)

compat.check("StringValue default and round-trip", function()
	local v = Instance.new("StringValue")
	assert(v.Value == "")
	v.Value = "hello"
	assert(v.Value == "hello")
end)

compat.check("BoolValue default and round-trip", function()
	local v = Instance.new("BoolValue")
	assert(v.Value == false)
	v.Value = true
	assert(v.Value == true)
end)

compat.check("Value classes reject wrong types", function()
	local iv = Instance.new("IntValue")
	assert(not pcall(function() iv.Value = "nope" end))
	local nv = Instance.new("NumberValue")
	assert(not pcall(function() nv.Value = true end))
	local sv = Instance.new("StringValue")
	-- Numbers coerce to strings (standard Lua lua_isstring behavior).
	sv.Value = 123
	assert(sv.Value == "123")
	assert(not pcall(function() sv.Value = true end))
	local bv = Instance.new("BoolValue")
	assert(not pcall(function() bv.Value = 0 end))
end)

compat.check("Value Changed fires with property name", function()
	local v = Instance.new("StringValue")
	local got = nil
	v.Changed:Connect(function(k) got = k end)
	v.Value = "ping"
	task.wait(0.1)
	assert(got == "Value")
end)

compat.check("Value classes IsA relationships", function()
	local v = Instance.new("IntValue")
	assert(v:IsA("IntValue"))
	assert(v:IsA("ValueBase"))
	assert(v:IsA("Instance"))
	assert(not v:IsA("Part"))
	local s = Instance.new("StringValue")
	assert(s:IsA("StringValue"))
	assert(s:IsA("ValueBase"))
	assert(s:IsA("Instance"))
	local b = Instance.new("BoolValue")
	assert(b:IsA("BoolValue"))
	assert(b:IsA("ValueBase"))
end)

compat.check("ValueBase is not directly creatable", function()
	assert(not pcall(function() Instance.new("ValueBase") end))
end)
