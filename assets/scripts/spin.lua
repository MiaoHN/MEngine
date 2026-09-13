-- Per-entity script: spins the owning entity around its Y axis.
function OnStart()
  MEngine.log("spin.lua: attached to '" .. self:get_name() .. "'")
end

function OnUpdate(dt)
  local rx, ry, rz = self:get_rotation()
  self:set_rotation(rx, ry + 90.0 * dt, rz)
end

function OnDestroy()
  MEngine.log("spin.lua: detached from '" .. self:get_name() .. "'")
end
