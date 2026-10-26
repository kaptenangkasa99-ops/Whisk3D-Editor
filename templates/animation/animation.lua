local elapsed = 0
local baseZ = 0.8
local spin = true

function update(dt)
    elapsed = elapsed + dt
    local player = object("Player")
    if not player then return end

    if isPressed(object("SpinButton")) then
        spin = not spin
    end

    local x, y, z = Position(player)
    z = baseZ + math.sin(elapsed * 2) * 0.3
    SetPosition(player, x, y, z)
    if spin then
        local rx, ry, rz = rotation(player)
        setRotation(player, rx, ry, (rz + 90 * dt) % 360)
    end
end
