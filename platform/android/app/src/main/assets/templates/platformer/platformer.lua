local speed = 3
local gravity = 9
local jumpSpeed = 5
local verticalSpeed = 0
local grounded = true
local jumpWasDown = false

local function fingerOver(objectRef)
    if not objectRef then return false end
    local cx, cy, width, height = getUIBox(objectRef)
    if width <= 0 or height <= 0 then return false end

    for index = 1, 4 do
        local tx, ty, active = finger(index)
        if active and math.abs(tx - cx) <= width * 0.5 and
           math.abs(ty - cy) <= height * 0.5 then
            return true
        end
    end
    return false
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = Position(player)
    local direction = 0
    if key("a") or key("left") then direction = direction - 1 end
    if key("d") or key("right") then direction = direction + 1 end
    if fingerOver(object("LeftButton")) then direction = direction - 1 end
    if fingerOver(object("RightButton")) then direction = direction + 1 end
    x = x + direction * speed * dt

    local jumpDown = fingerOver(object("JumpButton"))
    if grounded and (keyPressed("space") or (jumpDown and not jumpWasDown)) then
        verticalSpeed = jumpSpeed
        grounded = false
    end
    jumpWasDown = jumpDown
    if not grounded then
        verticalSpeed = verticalSpeed - gravity * dt
        z = z + verticalSpeed * dt
        if z <= 0.7 then
            z = 0.7
            verticalSpeed = 0
            grounded = true
        end
    end

    SetPosition(player, x, y, z)
end
