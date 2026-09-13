-- Logs each impact for the owning shape probe (speed via collision payload).
function OnCollisionEnter(other, hit)
  local name = other:get_name()
  if name == "" then return end
  local speed = 0.0
  if hit then
    local rv = hit.relative_velocity
    speed = math.sqrt(rv[1] * rv[1] + rv[2] * rv[2] + rv[3] * rv[3])
  end
  if speed < 0.05 then return end
  MEngine.log("impact: '" .. self:get_name() .. "' hit '" .. name .. "' speed=" .. string.format("%.2f", speed))
end
