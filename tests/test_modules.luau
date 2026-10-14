-- Tier 0: ModuleScript + require.
compat.check("require ModuleScript", function()
	local m = Instance.new("ModuleScript")
	m.Source = "return { answer = 42 }"
	local t = require(m)
	assert(t.answer == 42)
end)

compat.check("require caches result", function()
	local m = Instance.new("ModuleScript")
	m.Source = "calls = (calls or 0) + 1 return calls"
	local a = require(m)
	local b = require(m)
	assert(a == 1 and b == 1, "module executed more than once")
end)

compat.check("require non-module errors", function()
	local p = Instance.new("Part")
	local ok = pcall(function() require(p) end)
	assert(not ok)
end)

compat.check("script global", function()
	assert(script ~= nil)
	assert(script.ClassName == "Script")
end)
