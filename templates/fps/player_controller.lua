local moveSpeed = 4.5
local turnSpeed = 2.4
local fireInterval = 0.18

local aimAngle = 0
local fireCooldown = 0
local damageSeen = 0
local playerHealth = 5
local aimPitch = 0

local function get(name)
    return object(name)
end

local function setUpBody(obj, w, h, d)
    if not obj then return end
    collisionBox(obj, w, h, d)
    velocity(obj, 0, 0, 0)
end

function start()
    local player = get("Player")
    if not player then
        error("FPS player controller: missing Player object")
        return
    end
    setUpBody(player, 0.65, 1.8, 0.65)
    setVisible(player, false)

    local floor = get("Floor")
    if floor then collisionBox(floor, 24, 0.5, 24) end
    for _, name in ipairs({ "WallNorth", "WallSouth", "WallEast", "WallWest" }) do
        local wall = get(name)
        if wall then
            local isSideWall = name == "WallEast" or name == "WallWest"
            collisionBox(wall, isSideWall and 0.6 or 22,
                         3, isSideWall and 22 or 0.6)
        end
    end

    setShared("fps.fireSequence", 0)
    setShared("fps.damageSequence", 0)
    setShared("fps.resetRound", 0)
    setShared("fps.kills", 0)
    setShared("fps.aimX", 0)
    setShared("fps.aimY", 0)
end

local function resetPlayer(player)
    playerHealth = 5
    SetPosition(player, 0, 0.9, 0)
    setShared("fps.resetRound", (shared("fps.resetRound") or 0) + 1)
    setShared("fps.kills", 0)
    info("Round restarted")
end

function update(dt)
    dt = clamp(dt, 0, 0.05)
    local player = get("Player")
    if not player then return end

    fireCooldown = math.max(0, fireCooldown - dt)
    local damageSequence = shared("fps.damageSequence") or 0
    if damageSequence > damageSeen then
        playerHealth = math.max(0, playerHealth - (damageSequence - damageSeen))
        damageSeen = damageSequence
        info("Player health:", playerHealth)
        if playerHealth == 0 then resetPlayer(player) end
    end

    local moveX, moveY = stick("izq")
    if key("a") or key("left") then moveX = moveX - 1 end
    if key("d") or key("right") then moveX = moveX + 1 end
    if key("w") or key("up") then moveY = moveY - 1 end
    if key("s") or key("down") then moveY = moveY + 1 end
    if isDown(get("MoveLeft")) then moveX = moveX - 1 end
    if isDown(get("MoveRight")) then moveX = moveX + 1 end
    if isDown(get("MoveForward")) then moveY = moveY - 1 end
    if isDown(get("MoveBack")) then moveY = moveY + 1 end

    local magnitude = math.sqrt(moveX * moveX + moveY * moveY)
    if magnitude > 1 then moveX, moveY = moveX / magnitude, moveY / magnitude end

    local aimX, aimY = stick("der")
    if key("q") then aimX = aimX - 1 end
    if key("e") then aimX = aimX + 1 end
    if isDown(get("AimLeft")) then aimX = aimX - 1 end
    if isDown(get("AimRight")) then aimX = aimX + 1 end
    if isDown(get("AimUp")) then aimY = aimY - 1 end
    if isDown(get("AimDown")) then aimY = aimY + 1 end

    aimAngle = aimAngle + clamp(aimX, -1, 1) * turnSpeed * dt
    aimPitch = clamp(aimPitch - clamp(aimY, -1, 1) * turnSpeed * dt, -0.65, 0.65)
    local pitchScale = math.cos(aimPitch)
    local aimDirX = math.sin(aimAngle) * pitchScale
    local aimDirY = math.sin(aimPitch)
    local aimDirZ = math.cos(aimAngle) * pitchScale
    rotateTowards(player, aimDirX, aimDirZ, 1)
    setShared("fps.aimX", aimDirX)
    setShared("fps.aimY", aimDirY)
    setShared("fps.aimZ", aimDirZ)

    local forward = -moveY
    local worldX = moveX * math.cos(aimAngle) + forward * math.sin(aimAngle)
    local worldZ = -moveX * math.sin(aimAngle) + forward * math.cos(aimAngle)
    velocity(player, worldX * moveSpeed, 0, worldZ * moveSpeed)

    local px, py, pz = Position(player)
    local camera = get("Camera")
    if camera then
        SetPosition(camera, px, py + 1.35, pz)
        setRotation(camera, math.deg(aimPitch), math.deg(180 + aimAngle), 0)
    end
    local walls = { get("WallNorth"), get("WallSouth"),
                    get("WallEast"), get("WallWest") }
    bounce(player, walls)

    local shoot = key("space") or key("f") or button("a") or
                  isDown(get("FireButton"))
    if shoot and fireCooldown <= 0 then
        setShared("fps.fireSequence", (shared("fps.fireSequence") or 0) + 1)
        fireCooldown = fireInterval
    end
end
