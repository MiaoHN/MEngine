-- "Bouncer": a dynamic sphere that keeps bouncing on the floor.
-- Demonstrates collision data, apply_impulse and OnFixedUpdate.
-- Hold SPACE while in Play to add upward impulse every fixed step.

local function vec_len(x, y, z)
  return math.sqrt(x * x + y * y + z * z)
end

function OnStart()
  MEngine.log("bouncer ready; hold Space to hop")
  -- Initial upward velocity so it starts bouncing immediately.
  self:set_velocity(0.0, 8.0, 0.0)
end

function OnFixedUpdate(dt)
  -- Runs once per 1/60 s physics step, so the impulse is frame-rate independent.
  if MEngine.is_key_down("Space") then
    self:apply_impulse(0.0, 1.6, 0.0)
  end
end

function OnCollisionEnter(other, hit)
  local name = other:get_name()
  if name ~= "Ground" then
    MEngine.log("bouncer touched '" .. name .. "'")
    return
  end

  local speed = 0.0
  local nx, ny, nz = 0.0, 0.0, 0.0
  if hit then
    local rv = hit.relative_velocity
    speed = vec_len(rv[1], rv[2], rv[3])
    nx, ny, nz = hit.normal[1], hit.normal[2], hit.normal[3]
  end
  MEngine.log("bouncer hit ground, impact=" .. string.format("%.2f", speed) ..
              " normal=(" .. string.format("%.2f", nx) .. "," .. string.format("%.2f", ny) .. "," ..
              string.format("%.2f", nz) .. ")")

  -- When the bounce has decayed, kick it back up so the demo keeps running.
  if speed < 1.5 then
    self:apply_impulse(0.0, 5.0, 0.0)
    MEngine.log("bouncer re-hop")
  end
end

function OnCollisionExit(other)
  if other:get_name() == "Ground" then
    MEngine.log("bouncer left the ground")
  end
end
