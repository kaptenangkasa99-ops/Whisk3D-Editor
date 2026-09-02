# Project template: one playable platformer level in Whisk3D

This is a complete template for making one small playable level inspired by Crash-style platformers.

## 1) Scene setup

Create the following objects in the editor:

- `Player`
- `CameraRig`
- `Ground`
- `EnemyPatrol`
- `Coin`
- `RespawnPoint`
- `Goal`

You do not need a huge scene. One small section is enough to test the gameplay loop.

---

## 2) Player script

Attach this script to the `Player` object:

```lua
properties = {
    speed = 6.5,
    jumpForce = 9.5,
    gravity = 24.0,
    groundY = 0.0,
    velY = 0.0,
    respawnX = 0.0,
    respawnY = 2.0,
    respawnZ = 0.0,
    health = 3
}

function start()
    setShared("playerHealth", property("health"))
    setShared("score", 0)
    print("Player ready")
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local move = 0.0

    if key("a") or key("left") then
        move = move - 1
    end
    if key("d") or key("right") then
        move = move + 1
    end

    if keyDown("space") and y <= property("groundY") then
        property("velY") = property("jumpForce")
        sound("sounds/jump.wav", 0.5, 1.0, false)
    end

    property("velY") = property("velY") - property("gravity") * dt
    y = y + property("velY") * dt

    if y < property("groundY") then
        y = property("groundY")
        property("velY") = 0
    end

    x = x + move * property("speed") * dt

    if move > 0 then
        setRotation(player, 0, 0, 0)
    elseif move < 0 then
        setRotation(player, 0, 180, 0)
    end

    setPosition(player, x, y, z)

    if y < -10 then
        setPosition(player, property("respawnX"), property("respawnY"), property("respawnZ"))
        setShared("playerHealth", (shared("playerHealth") or 3) - 1)
        sound("sounds/fail.wav", 0.7, 1.0, false)
    end
end
```

---

## 3) Camera script

Attach to `CameraRig`:

```lua
properties = {
    followSpeed = 5.0,
    offsetX = 8.0,
    offsetY = 4.0,
    offsetZ = 10.0
}

function update(dt)
    local camera = object("CameraRig")
    local player = object("Player")
    if not camera or not player then return end

    local px, py, pz = position(player)
    local cx, cy, cz = position(camera)

    local targetX = px + property("offsetX")
    local targetY = py + property("offsetY")
    local targetZ = pz + property("offsetZ")

    cx = cx + (targetX - cx) * property("followSpeed") * dt
    cy = cy + (targetY - cy) * property("followSpeed") * dt
    cz = cz + (targetZ - cz) * property("followSpeed") * dt

    setPosition(camera, cx, cy, cz)
end
```

---

## 4) Patrol enemy script

Attach to `EnemyPatrol`:

```lua
properties = {
    speed = 2.0,
    minX = -5.0,
    maxX = 5.0,
    direction = 1,
    damage = 1
}

function update(dt)
    local enemy = object("EnemyPatrol")
    local player = object("Player")
    if not enemy then return end

    local x, y, z = position(enemy)
    x = x + property("speed") * property("direction") * dt

    if x < property("minX") then
        x = property("minX")
        property("direction") = 1
    elseif x > property("maxX") then
        x = property("maxX")
        property("direction") = -1
    end

    setPosition(enemy, x, y, z)

    if player then
        local px, py, pz = position(player)
        local dx = px - x
        local dy = py - y
        local dz = pz - z
        local dist = math.sqrt(dx * dx + dy * dy + dz * dz)

        if dist < 1.3 then
            setShared("playerHealth", (shared("playerHealth") or 3) - property("damage"))
            sound("sounds/hurt.wav", 0.7, 1.0, false)
        end
    end
end
```

---

## 5) Coin script

Attach to `Coin`:

```lua
properties = {
    value = 10,
    collected = false
}

function update(dt)
    local player = object("Player")
    local coin = object("Coin")
    if not player or not coin then return end

    if property("collected") then
        setVisible(coin, false)
        return
    end

    local px, py, pz = position(player)
    local cx, cy, cz = position(coin)
    local d = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

    if d < 1.5 then
        property("collected") = true
        setShared("score", (shared("score") or 0) + property("value"))
        sound("sounds/coin.wav", 0.7, 1.0, false)
    end
end
```

---

## 6) Goal script

Attach to `Goal`:

```lua
properties = {
    winScore = 50
}

function update(dt)
    local player = object("Player")
    local goal = object("Goal")
    if not player or not goal then return end

    local px, py, pz = position(player)
    local gx, gy, gz = position(goal)
    local d = math.sqrt((px - gx)^2 + (py - gy)^2 + (pz - gz)^2)

    if d < 2.0 then
        local score = shared("score") or 0
        if score >= property("winScore") then
            info("Level clear")
            setShared("gameWon", true)
        else
            info("Need more coins")
        end
    end
end
```

---

## 7) Level manager script

Attach this to any object, for example an empty object named `LevelManager`:

```lua
properties = {
    lives = 3,
    score = 0,
    gameOver = false,
    gameWon = false
}

function start()
    setShared("score", 0)
    setShared("playerHealth", property("lives"))
end

function update(dt)
    local health = shared("playerHealth") or 0
    local score = shared("score") or 0

    if health <= 0 and not property("gameOver") then
        property("gameOver") = true
        warning("Game over")
    end

    if shared("gameWon") then
        property("gameWon") = true
        info("Victory")
    end

    property("score") = score
end
```

---

## 8) How to test the level

Use this exact test loop:

1. Press play.
2. Run to the right.
3. Jump over the gap or obstacle.
4. Touch the enemy.
5. Confirm the health decreases.
6. Collect the coin.
7. Reach the goal only after collecting enough points.
8. Adjust values until the flow feels smooth.

---

## 9) Tuning values

This is the most important part of making it feel right.

- Increase `speed` for faster feel.
- Increase `jumpForce` for higher jumps.
- Increase `gravity` to make jumps feel snappier.
- Increase `followSpeed` to make camera more responsive.
- Increase `EnemyPatrol.speed` for harder enemy pressure.

---

## 10) Debugging checklist

Before adding more features, verify all of these:

- `Player` object exists and is named exactly `Player`
- `CameraRig` exists and is named exactly `CameraRig`
- `Coin` is collected when close enough
- enemy patrol moves within its range
- player respawns when falling below the level
- score updates on collection
- goal only wins when coin count is enough

Use `print()` and `warning()` liberally while testing.

---

## 11) Good first playable milestone

Your first milestone should be this:

- player can move
- player can jump
- camera follows player
- enemy patrol exists
- coin collection works
- respawn works
- goal checks win condition

At that point, you already have a real small game loop.

---

## 12) Final advice

The fastest way to make a good game is to make one small section feel excellent, not a huge world.

Build this as a tiny, polished slice:

- one flat run section
- one jump gap
- one enemy patrol
- one coin pickup
- one goal

Once that works, you can extend it with more enemies, moving platforms, hazards, and checkpoints.

This is the correct beginner-to-prototype flow for Whisk3D.
