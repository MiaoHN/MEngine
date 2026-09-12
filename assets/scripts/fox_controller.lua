-- Fox controller: drive the fox with WASD / the arrow keys.
--
-- The walk cycle is a property of the animation, not of this script: the sprite
-- animation component runs it only while the fox actually moves
-- (`play_while_moving`), and parks it on frame 0 otherwise. So this script only
-- has to move the entity - the fox walks when it moves and stands still when it
-- stops.
--
-- Attach to the "Fox" entity (Lua Script component -> scripts/fox_controller.lua).

local speed = 3.0  -- world units per second

local function key(name)
  return MEngine.is_key_down(name)
end

function OnStart()
  MEngine.log("fox_controller.lua: WASD / arrows move '" .. self:get_name() .. "'")
end

function OnUpdate(dt)
  local dx = 0.0
  local dy = 0.0
  if key("a") or key("left") then dx = dx - 1.0 end
  if key("d") or key("right") then dx = dx + 1.0 end
  if key("s") or key("down") then dy = dy - 1.0 end
  if key("w") or key("up") then dy = dy + 1.0 end

  -- Normalise so diagonals are not faster, then integrate.
  local length = math.sqrt(dx * dx + dy * dy)
  if length > 0.0 then
    dx = dx / length
    dy = dy / length
  end

  local x, y, z = self:get_position()
  self:set_position(x + dx * speed * dt, y + dy * speed * dt, z)

  -- Face the direction of travel (the sheet artwork faces right).
  if dx < -0.0001 then
    self:set_sprite_flip_x(true)
  elseif dx > 0.0001 then
    self:set_sprite_flip_x(false)
  end
end
