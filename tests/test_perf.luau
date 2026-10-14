-- Performance regression check: touch detection must scale to thousands of
-- parts. 2000 anchored parts are spread over a 200x20x200 grid, then 120
-- real frames run while wall/CPU time is measured.
--
-- Two engine behaviors shape this test:
--  * task.wait() with no delay resumes inside the SAME scheduler step, so it
--    does not advance frames; task.wait(0.02) is used to step real frames.
--  * Every property set fires Changed, so creation yields with a positive
--    delay every 100 parts to keep per-frame event bursts well under the
--    signal recursion guard (1000 fires/frame).
compat.check("touchCheck scales to 2000 parts", function()
	local idx = 0
	for gx = 0, 19 do
		for gy = 0, 4 do
			for gz = 0, 19 do
				if idx >= 2000 then break end
				idx = idx + 1
				local p = Instance.new("Part")
				p.Name = "PerfPart" .. idx
				p.Size = Vector3.new(1, 1, 1)
				p.Position = Vector3.new(gx * 10, gy * 10 + 5, gz * 10)
				p.Anchored = true
				p.Parent = workspace
				if idx % 100 == 0 then
					task.wait(0.05)
				end
			end
			if idx >= 2000 then break end
		end
		if idx >= 2000 then break end
	end
	assert(idx == 2000, "expected 2000 parts, got " .. idx)
	-- Settle: one full frame runs the sweep over all 2000 parts.
	task.wait(0.05)
	local t0 = os.clock()
	for i = 1, 60 do
		task.wait(0.02) -- ~2 frames per wait -> ~120 measured frames
	end
	local elapsed = os.clock() - t0
	assert(elapsed < 3.0, "120 frames with 2000 parts took " .. elapsed .. "s (> 3s)")
end)
