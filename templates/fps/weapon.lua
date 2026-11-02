local projectileSpeed = 18
local bulletCount = 8
local bullets = {}
local bulletLife = {}
local bulletActive = {}
local nextBullet = 1
local lastFireSequence = 0
local flashTime = 0
local trailTime = 0

local function get(name)
    return object(name)
end

local function stopBody(obj)
    if obj then velocity(obj, 0, 0, 0) end
end

local function setBulletActive(index, active)
    bulletActive[index] = active
    setVisible(bullets[index], active)
    if not active then stopBody(bullets[index]) end
end

local function fire()
    local selected = nil
    for offset = 0, bulletCount - 1 do
        local index = ((nextBullet + offset - 1) % bulletCount) + 1
        if not bulletActive[index] or bulletLife[index] <= 0 then
            selected = index
            break
        end
    end
    if not selected then return false end

    nextBullet = (selected % bulletCount) + 1
    local player = get("Player")
    if not player then return false end
    local px, py, pz = Position(player)
    local dx = shared("fps.aimX") or 0
    local dy = shared("fps.aimY") or 0
    local dz = shared("fps.aimZ") or 1
    local length = math.sqrt(dx * dx + dy * dy + dz * dz)
    if length > 0 then dx, dy, dz = dx / length, dy / length, dz / length end

    local bullet = bullets[selected]
    SetPosition(bullet, px + dx * 0.85, py + 0.9, pz + dz * 0.85)
    velocity(bullet, dx * projectileSpeed, dy * projectileSpeed, dz * projectileSpeed)
    setVisible(bullet, true)
    bulletActive[selected] = true
    bulletLife[selected] = 1.4

    local muzzle = get("MuzzleParticles")
    if muzzle then
        SetPosition(muzzle, px + dx * 0.65, py + 0.9, pz + dz * 0.65)
        emitter(muzzle, 8)
    end
    local flash = get("GunFlash")
    if flash then
        SetPosition(flash, px + dx * 0.5, py + 0.9, pz + dz * 0.5)
        setEnergy(flash, 1)
        flashTime = 0.06
    end
    return true
end

local function damageEnemy(index, enemy)
    local key = "fps.enemyHealth." .. index
    local health = (shared(key) or 3) - 1
    setShared(key, health)
    if health <= 0 then
        setShared("fps.enemyAlive." .. index, false)
        setVisible(enemy, false)
        stopBody(enemy)
        local kills = (shared("fps.kills") or 0) + 1
        setShared("fps.kills", kills)
        info("Enemy eliminated. Total:", kills)
    end
end

function start()
    for i = 1, bulletCount do
        bullets[i] = get(string.format("Bullet%02d", i))
        bulletLife[i] = 0
        bulletActive[i] = false
        if bullets[i] then
            collisionBox(bullets[i], 0.14, 0.14, 0.14)
            stopBody(bullets[i])
            setBulletActive(i, false)
        end
    end
    lastFireSequence = shared("fps.fireSequence") or 0
end

function update(dt)
    dt = clamp(dt, 0, 0.05)
    flashTime = math.max(0, flashTime - dt)
    trailTime = trailTime - dt

    local fireSequence = shared("fps.fireSequence") or 0
    if fireSequence > lastFireSequence then
        lastFireSequence = fireSequence
        fire()
    end

    local player = get("Player")
    local walls = { get("WallNorth"), get("WallSouth"),
                    get("WallEast"), get("WallWest") }
    for i = 1, bulletCount do
        local bullet = bullets[i]
        if bullet and bulletActive[i] then
            bulletLife[i] = bulletLife[i] - dt
            local hit = false
            for enemyIndex = 1, 3 do
                local enemy = get("Enemy0" .. enemyIndex)
                if enemy and shared("fps.enemyAlive." .. enemyIndex) ~= false and
                   bounce(bullet, enemy) then
                    damageEnemy(enemyIndex, enemy)
                    hit = true
                    break
                end
            end
            if not hit then
                for _, wall in ipairs(walls) do
                    if wall and bounce(bullet, wall) then hit = true; break end
                end
            end
            if hit or bulletLife[i] <= 0 then
                setBulletActive(i, false)
            elseif trailTime <= 0 then
                local trail = get("BulletTrail")
                if trail then
                    local bx, by, bz = Position(bullet)
                    SetPosition(trail, bx, by, bz)
                    emitter(trail, 1)
                    trailTime = 0.035
                end
            end
        end
    end

    local flash = get("GunFlash")
    if flash and flashTime <= 0 then setEnergy(flash, 0) end
end
