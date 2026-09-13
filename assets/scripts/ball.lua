-- Attached to every Ball spawned by main.lua (at runtime via
-- add_component("lua_script", "scripts/ball.lua")).
-- Demonstrates the rich collision payload: other entity + collision table with
-- point / normal / relative_velocity / penetration, plus OnCollisionExit.

local age = 0.0
local LIFETIME = 4.5  -- self-destruct so the arena never floods

local function vec_len(x, y, z)
  return math.sqrt(x * x + y * y + z * z)
end

function OnStart()
  MEngine.log("'" .. self:get_name() .. "' is live")
end

function OnUpdate(dt)
  age = age + dt
  if age >= LIFETIME then
    MEngine.destroy_entity(self)
  end
end

function OnCollisionEnter(other, hit)
  local name = other:get_name()
  if name == "" then return end  -- the other entity was already destroyed

  local speed = 0.0
  local px, py, pz = 0.0, 0.0, 0.0
  local nx, ny, nz = 0.0, 0.0, 0.0
  if hit then
    local pt = hit.point
    local n = hit.normal
    local rv = hit.relative_velocity
    px, py, pz = pt[1], pt[2], pt[3]
    nx, ny, nz = n[1], n[2], n[3]
    speed = vec_len(rv[1], rv[2], rv[3])
  end

  -- Ignore resting/chatter contacts (e.g. the spawn touching the ground).
  if speed < 0.3 then return end

  MEngine.log(self:get_name() .. " hit '" .. name .. "'  speed=" .. string.format("%.2f", speed) ..
              "  point=(" .. string.format("%.2f", px) .. "," .. string.format("%.2f", py) .. "," ..
              string.format("%.2f", pz) .. ")  normal=(" .. string.format("%.2f", nx) .. "," ..
              string.format("%.2f", ny) .. "," .. string.format("%.2f", nz) .. ")")
end

function OnCollisionExit(other)
  local name = other:get_name()
  if name ~= "" then
    MEngine.log(self:get_name() .. " exited '" .. name .. "'")
  end
end
