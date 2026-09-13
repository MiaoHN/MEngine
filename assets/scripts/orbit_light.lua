-- Orbits the owning (light) entity around the scene origin in the XZ plane and
-- spins it on its own Y axis. Each light keeps the radius / direction / height
-- it starts at, so a group of lights rotates around the centre together (their
-- formation is preserved). Press Play in the editor to run it.
local radius    = 0.0
local base_ang  = 0.0
local base_y    = 0.0
local time      = 0.0
local speed     = 0.5    -- radians per second around the scene centre
local spin      = 120.0  -- degrees per second on the entity's own Y axis
local bob_amp   = 0.25   -- vertical bob amplitude
local bob_speed = 1.3    -- bob cycles per second

function OnStart()
  local x, y, z = self:get_position()
  radius   = math.sqrt(x * x + z * z)
  base_ang = math.atan2(z, x)
  base_y   = y
end

function OnUpdate(dt)
  time = time + dt

  -- Orbit around the scene centre at the light's own radius/phase.
  local a = base_ang + speed * time
  local y = base_y + bob_amp * math.cos(bob_speed * time * 6.2831853)
  self:set_position(radius * math.cos(a), y, radius * math.sin(a))

  -- Spin the entity (and its emissive cube) on its own Y axis.
  local rx, ry, rz = self:get_rotation()
  self:set_rotation(rx, ry + spin * dt, rz)
end
