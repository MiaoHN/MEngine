-- physics_test scene driver (loaded standalone). Verifies raycast + shape drops.
local ray_timer = 0.0

function OnStart()
  MEngine.log("physics_test: started; entities=" .. #MEngine.get_entities())
end

function OnUpdate(dt)
  ray_timer = ray_timer + dt
  if ray_timer >= 1.0 then
    ray_timer = 0.0
    local hit, dist = MEngine.raycast(0.0, 10.0, 0.0, 0.0, -1.0, 0.0, 25.0)
    if hit ~= nil then
      MEngine.log("physics_test: raycast hit '" .. hit:get_name() .. "' at " .. string.format("%.2f", dist))
    else
      MEngine.log("physics_test: raycast missed")
    end
  end
end
