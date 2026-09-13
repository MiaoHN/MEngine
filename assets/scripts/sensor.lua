-- Sensor (trigger) probe: counts bodies entering/leaving the sensor volume.
local inside = 0

function OnCollisionEnter(other, hit)
  local name = other:get_name()
  if name == "" then return end
  inside = inside + 1
  MEngine.log("sensor: '" .. name .. "' entered (" .. inside .. " inside)")
end

function OnCollisionExit(other)
  local name = other:get_name()
  if name == "" then return end
  inside = inside - 1
  if inside < 0 then inside = 0 end
  MEngine.log("sensor: '" .. name .. "' left (" .. inside .. " inside)")
end
