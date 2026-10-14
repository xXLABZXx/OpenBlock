-- OpenBlock render demo: builds a small 3D scene for the --screenshot flag.
-- Run: openblox scripts/render_demo.luau --screenshot /tmp/demo.ppm
-- (The default camera looks at (0,4,0) from (0,14,26), so keep the interesting
-- geometry near the origin and above the baseplate.)
print("OpenBlock render demo: building scene")

local function makePart(name, size, pos, color)
    local p = Instance.new("Part")
    p.Name = name
    p.Size = size
    p.Position = pos
    p.Color = color
    p.Anchored = true
    p.Parent = game.Workspace
    return p
end

-- Baseplate
makePart("Baseplate", Vector3.new(64, 1, 64), Vector3.new(0, -0.5, 0),
    Color3.fromRGB(70, 95, 70))

-- Colored blocks
makePart("RedTower", Vector3.new(4, 10, 4), Vector3.new(-9, 5, -2),
    Color3.fromRGB(230, 40, 40))
makePart("BlueBlock", Vector3.new(7, 3, 7), Vector3.new(7, 1.5, -3),
    Color3.fromRGB(40, 90, 230))
makePart("YellowPillar", Vector3.new(2, 12, 2), Vector3.new(1, 6, 6),
    Color3.fromRGB(240, 215, 50))
makePart("TopSlab", Vector3.new(10, 1, 10), Vector3.new(-9, 10.5, -2),
    Color3.fromRGB(205, 205, 215))

-- A ball: until the engine models shapes, this renders as a shaded disc.
makePart("Ball", Vector3.new(3, 3, 3), Vector3.new(9, 1.5, 7),
    Color3.fromRGB(40, 205, 80))

print("Scene ready: " .. tostring(#game.Workspace:GetChildren()) .. " children in Workspace")
