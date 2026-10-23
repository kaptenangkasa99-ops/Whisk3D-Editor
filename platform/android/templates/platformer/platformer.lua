local speed = 3
local gravity = 9
local jumpSpeed = 5
local verticalSpeed = 0
local grounded = true

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = Position(player)
    local direction = 0
    if key("a") or key("left") then direction = direction - 1 end
    if key("d") or key("right") then direction = direction + 1 end
    if isPressed(object("LeftButton")) then direction = direction - 1 end
    if isPressed(object("RightButton")) then direction = direction + 1 end
    x = x + direction * speed * dt

    if grounded and (keyPressed("space") or isPressed(object("JumpButton"))) then
        verticalSpeed = jumpSpeed
        grounded = false
    end
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
