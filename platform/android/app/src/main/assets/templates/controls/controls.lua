local speed = 3

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = Position(player)
    local direction = 0
    if key("a") or key("left") then direction = direction - 1 end
    if key("d") or key("right") then direction = direction + 1 end
    if isDown(object("LeftButton")) then direction = direction - 1 end
    if isDown(object("RightButton")) then direction = direction + 1 end
    x = x + direction * speed * dt
    if isPressed(object("JumpButton")) or keyPressed("space") then
        z = z < 1.2 and 1.5 or 0.8
    end
    SetPosition(player, x, y, z)
end
