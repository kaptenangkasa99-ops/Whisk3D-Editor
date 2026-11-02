local enemySpeed = 1.15
local damageCooldown = 0
local observedReset = 0
local enemyHealth = 3

local function get(name)
    return object(name)
end

local function setUpEnemy(enemy)
    if enemy then
        collisionBox(enemy, 0.8, 1.8, 0.8)
        velocity(enemy, 0, 0, 0)
        setVisible(enemy, true)
    end
end

function start()
    for _, name in ipairs({ "WallNorth", "WallSouth", "WallEast", "WallWest" }) do
        local wall = get(name)
        if wall then
            local side = name == "WallEast" or name == "WallWest"
            collisionBox(wall, side and 0.6 or 22, 3, side and 22 or 0.6)
        end
    end
    for i = 1, 3 do
        setShared("fps.enemyHealth." .. i, enemyHealth)
        setShared("fps.enemyAlive." .. i, true)
        setUpEnemy(get("Enemy0" .. i))
    end
    observedReset = shared("fps.resetRound") or 0
end

function update(dt)
    dt = clamp(dt, 0, 0.05)
    damageCooldown = math.max(0, damageCooldown - dt)
    local reset = shared("fps.resetRound") or 0
    if reset ~= observedReset then
        observedReset = reset
        for i = 1, 3 do
            setShared("fps.enemyHealth." .. i, enemyHealth)
            setShared("fps.enemyAlive." .. i, true)
            setUpEnemy(get("Enemy0" .. i))
        end
    end

    local player = get("Player")
    if not player then return end
    local px, _, pz = Position(player)
    local walls = { get("WallNorth"), get("WallSouth"),
                    get("WallEast"), get("WallWest") }
    for i = 1, 3 do
        local enemy = get("Enemy0" .. i)
        if enemy and shared("fps.enemyAlive." .. i) ~= false then
            local ex, _, ez = Position(enemy)
            local dx, dz = px - ex, pz - ez
            local distance = math.sqrt(dx * dx + dz * dz)
            if distance > 1.25 then
                dx, dz = dx / distance, dz / distance
                velocity(enemy, dx * enemySpeed, 0, dz * enemySpeed)
                rotateTowards(enemy, dx, dz, clamp(dt * 3, 0, 1))
            else
                velocity(enemy, 0, 0, 0)
                if damageCooldown <= 0 then
                    setShared("fps.damageSequence",
                              (shared("fps.damageSequence") or 0) + 1)
                    damageCooldown = 0.8
                end
            end
            bounce(enemy, walls)
            bounce(enemy, player)
        end
    end
end
