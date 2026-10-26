local speed = 3
local gravity = 9
local jumpSpeed = 5
local verticalSpeed = 0
local grounded = true

function update(dt)
    local player = object("Player")
    if not player then return end

    -- Whisk3D uses Y-up world coordinates (the camera's worldUp is 0,1,0).
    local x, y, z = Position(player)
    local direction = 0
    if key("a") or key("left") then direction = direction - 1 end
    if key("d") or key("right") then direction = direction + 1 end
    if isDown(object("LeftButton")) then direction = direction - 1 end
    if isDown(object("RightButton")) then direction = direction + 1 end
    x = x + direction * speed * dt

    local jumpButton = object("JumpButton")
    if grounded and (keyPressed("space") or keyPressed("w") or
                     isPressed(jumpButton)) then
        verticalSpeed = jumpSpeed
        grounded = false
    end
    if not grounded then
        verticalSpeed = verticalSpeed - gravity * dt
        y = y + verticalSpeed * dt
        if y <= 0.7 then
            y = 0.7
            verticalSpeed = 0
            grounded = true
        end
    end

    SetPosition(player, x, y, z)
end
