-- Scene-level main script = the collision demo driver.
-- Every SPAWN_INTERVAL seconds it builds a dynamic "Ball" from scratch using
-- the runtime Lua API (create_entity / add_component / set_color /
-- set_velocity) and launches it down the +X lane into the static Targets.
-- When every Target has been destroyed, they are rebuilt after a pause.

local SPAWN_INTERVAL = 1.5
local RESPAWN_PAUSE = 1.2
local spawn_timer = 0.0
local respawn_timer = 0.0
local ball_count = 0

local TARGET_LAYOUT = {
  { x = -3.0, r = 0.25, g = 0.55, b = 0.95 },
  { x = 0.0, r = 0.25, g = 0.78, b = 0.42 },
  { x = 3.0, r = 0.95, g = 0.62, b = 0.18 },
}

function OnStart()
  MEngine.log("main.lua: collision demo started")
end

local function spawn_target(x, r, g, b)
  local e = MEngine.create_entity("Target")
  e:add_component("transform")
  e:set_position(x, 0.5, 0.0)
  e:add_component("mesh", "cube")
  e:set_color(r, g, b, 1.0)
  e:add_component("collider", "box")
  e:add_component("rigid_body", "static")
  e:add_component("lua_script", "scripts/target.lua")
  return e
end

local function spawn_ball()
  ball_count = ball_count + 1
  local e = MEngine.create_entity("Ball " .. ball_count)
  e:add_component("transform")
  e:set_position(-9.0, 0.5, (math.random() - 0.5) * 0.6)
  e:add_component("mesh", "sphere")
  local shade = 0.75 + math.random() * 0.25
  e:set_color(shade, shade, 1.0, 1.0)
  e:add_component("collider", "sphere", 0.5)
  -- dynamic, low friction, bouncy -> knocks and bounces off the targets
  e:add_component("rigid_body", "dynamic", 0.4, 0.75)
  e:add_component("lua_script", "scripts/ball.lua")
  e:set_velocity(8.5 + math.random() * 3.0, math.random() * 0.6 - 0.2, 0.0)
  MEngine.log("launched '" .. e:get_name() .. "' down the lane")
end

function OnUpdate(dt)
  spawn_timer = spawn_timer + dt
  if spawn_timer >= SPAWN_INTERVAL then
    spawn_timer = 0.0
    spawn_ball()
  end

  -- Rebuild the target row once it has been entirely shot down.
  if MEngine.find_entity("Target") == nil then
    respawn_timer = respawn_timer + dt
    if respawn_timer >= RESPAWN_PAUSE then
      respawn_timer = 0.0
      MEngine.log("main.lua: rebuilding target row")
      for _, t in ipairs(TARGET_LAYOUT) do
        spawn_target(t.x, t.r, t.g, t.b)
      end
    end
  else
    respawn_timer = 0.0
  end
end
