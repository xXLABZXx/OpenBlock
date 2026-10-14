-- OpenBlox demo script: exercises the prototype DataModel, events,
-- task scheduler, Vector3 math, modules, and the sandbox.
print("OpenBlox demo starting")

-- 1. Instances, properties, hierarchy ---------------------------------------
local baseplate = Instance.new("Part")
baseplate.Name = "Baseplate"
baseplate.Size = Vector3.new(512, 1, 512)
baseplate.Position = Vector3.new(0, -0.5, 0)
baseplate.Anchored = true
baseplate.Parent = game.Workspace

print("Created:", baseplate:GetFullName())
print("Size:", baseplate.Size, "Anchored:", baseplate.Anchored)
print("IsA Part?", baseplate:IsA("Part"), "IsA BasePart?", baseplate:IsA("BasePart"))

-- 2. Changed event ------------------------------------------------------------
local changes = 0
local conn = baseplate.Changed:Connect(function(prop)
    changes += 1
    print("Changed fired for property:", prop, "(total:", changes .. ")")
end)
print("Connection live?", conn.Connected)

-- 3. Heartbeat ------------------------------------------------------------------
local RunService = game:GetService("RunService")
local frames = 0
RunService.Heartbeat:Connect(function(dt)
    frames += 1
    if frames % 120 == 0 then
        print(string.format("Heartbeat: %d frames, dt=%.4f", frames, dt))
    end
end)

-- 4. task.spawn + task.wait -------------------------------------------------------
task.spawn(function()
    for i = 1, 3 do
        task.wait(1.0)
        baseplate.Transparency = i * 0.2
        print("spawned task tick", i, "-> transparency", baseplate.Transparency)
    end
    conn:Disconnect()
    print("Changed connection live after disconnect?", conn.Connected)
end)

-- 5. task.delay ---------------------------------------------------------------------
task.delay(2.5, function()
    print("delayed hello at t=" .. string.format("%.2f", tick()))
end)

-- 6. Vector3 math ----------------------------------------------------------------------
local v = Vector3.new(1, 2, 3) + Vector3.new(4, 5, 6)
print("Vector3 add:", v, "magnitude:", string.format("%.3f", v.Magnitude))
print("Vector3 scaled:", v * 2)

-- 7. ModuleScript + require ---------------------------------------------------------------
local util = Instance.new("ModuleScript")
util.Name = "MathUtil"
util.Source = [[
local M = {}
function M.double(x) return x * 2 end
return M
]]
util.Parent = game:GetService("ReplicatedStorage")
local mathUtil = require(util)
print("require(MathUtil).double(21) =", mathUtil.double(21))

-- 8. Sandbox check: dangerous capabilities must be absent ----------------------------------------
print("os.execute present?", os.execute ~= nil)
print("dofile present?", dofile ~= nil)
print("debug library present?", debug ~= nil)
print("os.clock still works?", os.clock() >= 0)

-- 9. Error handling: unknown class / member ----------------------------------------------------------
local ok1 = pcall(function() Instance.new("NoSuchClass") end)
print("Instance.new('NoSuchClass') errors?", not ok1)
local ok2 = pcall(function() baseplate.BogusMember = 1 end)
print("setting unknown member errors?", not ok2)

print("OpenBlox demo script finished scheduling; Heartbeat keeps running")
