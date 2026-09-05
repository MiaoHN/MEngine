-- Scene-level main script (optional "GameManager").
function OnStart()
  MEngine.log("main.lua: started, " .. #MEngine.get_entities() .. " entities in scene")
end

function OnUpdate(dt)
  -- Heartbeat once per second, just to prove the hook keeps running.
  if math.floor(MEngine.time()) ~= math.floor(MEngine.time() - dt) then
    MEngine.log("main.lua: tick at t=" .. string.format("%.1f", MEngine.time()))
  end
end
