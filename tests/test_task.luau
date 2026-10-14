-- Tier 0: task library.
compat.check("task.spawn runs", function()
	local ran = false
	task.spawn(function() ran = true end)
	task.wait(0.1)
	assert(ran)
end)

compat.check("task.spawn passes args", function()
	local got = nil
	task.spawn(function(a, b) got = a + b end, 2, 3)
	task.wait(0.1)
	assert(got == 5)
end)

compat.check("task.spawn returns thread", function()
	local th = task.spawn(function() end)
	assert(th ~= nil)
end)

compat.check("task.delay", function()
	local t0 = tick()
	local firedAt = nil
	task.delay(0.3, function() firedAt = tick() end)
	task.wait(0.5)
	assert(firedAt ~= nil and firedAt - t0 >= 0.29, "delay fired at wrong time")
end)

compat.check("task.wait yields time", function()
	local t0 = tick()
	task.wait(0.25)
	local dt = tick() - t0
	assert(dt >= 0.24 and dt < 1.0, "waited " .. dt)
end)

compat.check("task.cancel", function()
	local ran = false
	local th = task.delay(0.2, function() ran = true end)
	task.cancel(th)
	task.wait(0.4)
	assert(not ran)
end)

compat.check("task.defer runs after current", function()
	local order = {}
	task.defer(function() table.insert(order, "deferred") end)
	table.insert(order, "now")
	task.wait(0.1)
	assert(order[1] == "now" and order[2] == "deferred")
end)
