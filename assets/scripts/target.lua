-- Static brick target. Each impact (measured via hit.relative_velocity) costs
-- 1 HP: the brick blends toward red and is destroyed at 0 HP. Also shows
-- has_component / get_color / set_color / destroy_entity.

local MAX_HP = 3
local hp = MAX_HP
local base = { 1.0, 1.0, 1.0 }

local function vec_len(x, y, z)
  return math.sqrt(x * x + y * y + z * z)
end

function OnStart()
  local r, g, b = self:get_color()
  base = { r, g, b }
end

function OnCollisionEnter(other, hit)
  -- Only care about a moving body actually striking us.
  if not other:has_component("rigid_body") then return end

  local speed = 0.0
  if hit then
    local rv = hit.relative_velocity
    speed = vec_len(rv[1], rv[2], rv[3])
  end
  if speed < 0.6 then return end  -- resting contact / chatter

  hp = hp - 1
  local t = math.max(0.0, hp / MAX_HP)  -- 1 = healthy, 0 = dead
  self:set_color(base[1] * t + 0.9 * (1 - t),
                 base[2] * t + 0.08 * (1 - t),
                 base[3] * t + 0.08 * (1 - t), 1.0)
  MEngine.log("'" .. self:get_name() .. "' hit! hp=" .. hp ..
              " impact=" .. string.format("%.2f", speed))

  if hp <= 0 then
    MEngine.log("'" .. self:get_name() .. "' destroyed")
    MEngine.destroy_entity(self)
  end
end
