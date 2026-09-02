# Crash-style platformer guide for Whisk3D

This guide shows how to make a basic 3D platformer in Whisk3D with:

- running and jumping locomotion
- camera follow
- enemy AI patterns
- hazards and pickups
- playtesting and debugging
- build/deploy workflow

The target style is inspired by Crash Bandicoot: fast platforming, grounded movement, short jumps, enemy patrols, and clear challenge pacing.

---

## 1) How the editor works in Whisk3D

Whisk3D uses Lua scripts attached to objects. The important idea is:

- create a player object
- attach a Lua script to it
- define `properties` for editable values
- use `start()` for setup
- use `update(dt)` for movement and logic
- use `object("Name")` to reference other objects

Minimal script pattern:

```lua
properties = {
    speed = 6.0,
    jumpForce = 9.0,
    gravity = 18.0,
    label = "Player"
}

function start()
    print("Player started")
end

function update(dt)
    -- every frame
end
```

### Editor workflow

1. Create the main scene.
2. Add a player mesh or capsule object.
3. Rename it to something clear, like `Player`.
4. Add a Lua script to the object.
5. Set editable values in the inspector using `properties`.
6. Press play to test.
7. If something is wrong, use `print()`, `debug()`, `warning()`, and `error()`.

### Naming convention

Use names that are easy to reference:

- `Player`
- `CameraRig`
- `EnemyPatrol1`
- `PickupCoin1`
- `CheckpointA`

You can then access them with:

```lua
local player = object("Player")
local camera = object("CameraRig")
local enemy = object("EnemyPatrol1")
```

---

## 2) Basic locomotion: running, jumping, grounded movement

The easiest Crash-like movement is a grounded platformer controller with:

- left/right movement
- acceleration and friction
- jump when grounded
- gravity while airborne
- simple camera follow

### Player controller example

```lua
properties = {
    speed = 6.0,
    acceleration = 18.0,
    friction = 14.0,
    jumpForce = 9.0,
    gravity = 24.0,
    groundY = 0.0,
    isGrounded = false,
    moveX = 0.0,
    velocityY = 0.0
}

function start()
    local p = object("Player")
    if p then
        local x, y, z = position(p)
        setPosition(p, x, property("groundY"), z)
    end
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local move = 0.0

    if key("a") or key("left") then
        move = move - 1.0
    end
    if key("d") or key("right") then
        move = move + 1.0
    end

    -- horizontal movement with acceleration and friction
    local currentX = x
    local targetX = currentX + move * property("speed") * dt

    if move == 0 then
        targetX = currentX
    end

    -- keep the player on the ground if grounded
    if y <= property("groundY") then
        y = property("groundY")
        setShared("playerGrounded", true)
        property("isGrounded") = true

        if keyDown("space") then
            property("velocityY") = property("jumpForce")
            property("isGrounded") = false
            sound("sounds/jump.wav", 0.5, 1.0, false)
        end
    else
        property("isGrounded") = false
    end

    -- gravity
    property("velocityY") = property("velocityY") - property("gravity") * dt
    y = y + property("velocityY") * dt

    if y < property("groundY") then
        y = property("groundY")
        property("velocityY") = 0
        property("isGrounded") = true
    end

    -- face direction
    if move > 0 then
        setRotation(player, 0, 0, 0)
    elseif move < 0 then
        setRotation(player, 0, 180, 0)
    end

    setPosition(player, targetX, y, z)
end
```

### Notes

This is a very good starting point because it is:

- easy to understand
- simple to debug
- close to Crash-like platforming
- easy to tune later

For a more polished version, add:

- coyote time
- variable jump height
- jump buffering
- attack hitbox
- double jump only if unlocked
- stomp detection on enemies

---

## 3) Better locomotion: more Crash-like feel

Crash platformers are not just jump+move. They feel good because:

- movement is responsive
- run speed is steady
- jump arcs feel precise
- landing frames are readable
- villain collisions are forgiving

A better version uses a small state machine.

```lua
properties = {
    speed = 7.5,
    runAcceleration = 30.0,
    groundFriction = 18.0,
    airControl = 0.4,
    jumpForce = 9.5,
    gravity = 26.0,
    maxFallSpeed = 20.0,
    coyoteTime = 0.12,
    jumpBuffer = 0.12,
    groundY = 0.0
}

function start()
    setShared("playerOnGround", true)
    setShared("playerJumpQueued", false)
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local moveInput = 0.0

    if key("a") or key("left") then moveInput = moveInput - 1 end
    if key("d") or key("right") then moveInput = moveInput + 1 end

    local vx = 0.0
    local grounded = (y <= property("groundY"))

    if grounded then
        setShared("playerOnGround", true)
    else
        setShared("playerOnGround", false)
    end

    if grounded then
        if moveInput ~= 0 then
            vx = moveInput * property("speed")
            if moveInput > 0 then
                setRotation(player, 0, 0, 0)
            else
                setRotation(player, 0, 180, 0)
            end
        else
            vx = 0
        end
    else
        vx = moveInput * property("speed") * property("airControl")
    end

    if keyDown("space") then
        setShared("playerJumpQueued", true)
    end

    if grounded and (shared("playerJumpQueued") or keyDown("space")) then
        local jumpVel = property("jumpForce")
        setShared("playerJumpQueued", false)
        -- use a shared variable or property as vertical velocity
        setShared("playerVelY", jumpVel)
        sound("sounds/jump.wav", 0.4, 1.0, false)
    end

    local velY = shared("playerVelY") or 0
    velY = velY - property("gravity") * dt
    if velY < -property("maxFallSpeed") then
        velY = -property("maxFallSpeed")
    end

    y = y + velY * dt
    if y < property("groundY") then
        y = property("groundY")
        velY = 0
        setShared("playerOnGround", true)
    end

    setShared("playerVelY", velY)
    setPosition(player, x + vx * dt, y, z)
end
```

### Why this works

This version is closer to a quality platformer controller because it keeps the feeling of:

- controlled acceleration
- tighter jump arcs
- good air control
- clear grounded state

---

## 4) Camera follow for a Crash-style game

In a Crash-like game, the camera should:

- follow the player horizontally
- stay behind or in front depending on direction
- not rotate wildly
- be predictable and forgiving

A good first version is a simple follow camera.

```lua
properties = {
    followSpeed = 5.0,
    offsetX = 8.0,
    offsetY = 4.0,
    offsetZ = 10.0,
    lookAhead = 2.0
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

    local dx = targetX - cx
    local dy = targetY - cy
    local dz = targetZ - cz

    cx = cx + dx * property("followSpeed") * dt
    cy = cy + dy * property("followSpeed") * dt
    cz = cz + dz * property("followSpeed") * dt

    setPosition(camera, cx, cy, cz)
end
```

### Rules for a good platforming camera

- keep the camera a little behind the player
- do not over-rotate around the player
- keep the camera in front of jumps so landing is readable
- if the level is vertical, also move the camera upward smoothly

---

## 5) Common level challenges in a Crash-style game

Crash games are built from repeated challenge patterns, not just random obstacles. The core challenge shapes are:

### 5.1 Jumps over gaps

Use a ground object and a gap between platforms.

```lua
properties = {
    safeGroundY = 0.0,
    failY = -10.0,
    respawnX = 0.0,
    respawnY = 2.0,
    respawnZ = 0.0
}

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    if y < property("failY") then
        setPosition(player, property("respawnX"), property("respawnY"), property("respawnZ"))
        setShared("playerLives", (shared("playerLives") or 3) - 1)
        sound("sounds/fail.wav", 0.6, 1.0, false)
    end
end
```

### 5.2 Moving platforms

This pattern is very common in Crash-type levels.

```lua
properties = {
    amplitude = 3.0,
    speed = 1.2,
    axis = "x",
    phase = 0.0
}

function update(dt)
    local platform = object("MovingPlatform")
    if not platform then return end

    local x, y, z = position(platform)
    local t = property("phase") + dt * property("speed")
    local offset = math.sin(t) * property("amplitude")

    if property("axis") == "x" then
        setPosition(platform, x + offset, y, z)
    elseif property("axis") == "z" then
        setPosition(platform, x, y, z + offset)
    else
        setPosition(platform, x, y + offset, z)
    end

    property("phase") = t
end
```

### 5.3 Hazards

Use zones or objects that trigger instant failure.

```lua
properties = {
    hazardDamage = 1,
    respawnX = 0.0,
    respawnY = 2.0,
    respawnZ = 0.0
}

function update(dt)
    local player = object("Player")
    local hazard = object("SpikeTrap")
    if not player or not hazard then return end

    local px, py, pz = position(player)
    local hx, hy, hz = position(hazard)
    local distance = math.sqrt((px - hx)^2 + (py - hy)^2 + (pz - hz)^2)

    if distance < 2.0 then
        setPosition(player, property("respawnX"), property("respawnY"), property("respawnZ"))
        setShared("playerHealth", (shared("playerHealth") or 3) - property("hazardDamage"))
        sound("sounds/hurt.wav", 0.7, 1.0, false)
    end
end
```

### 5.4 Collectibles

```lua
properties = {
    coinValue = 10,
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
        setShared("score", (shared("score") or 0) + property("coinValue"))
        sound("sounds/coin.wav", 0.7, 1.0, false)
    end
end
```

---

## 6) Enemy AI patterns

The most useful enemy AI patterns are simple and readable. In Crash-like games, enemies are usually not hard AI monsters; they are challenge loops.

### 6.1 Patrol AI

This is the most basic enemy type: moves left and right between two points.

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

    -- simple collision with player
    local player = object("Player")
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

### 6.2 Chase AI

Enemy notices player and starts following.

```lua
properties = {
    patrolSpeed = 1.2,
    chaseSpeed = 3.0,
    detectionRange = 8.0,
    direction = 1
}

function update(dt)
    local enemy = object("EnemyChaser")
    local player = object("Player")
    if not enemy or not player then return end

    local ex, ey, ez = position(enemy)
    local px, py, pz = position(player)
    local dx = px - ex
    local dist = math.sqrt(dx * dx)

    local speed = property("patrolSpeed")
    if dist < property("detectionRange") then
        speed = property("chaseSpeed")
        if dx < 0 then
            property("direction") = -1
        else
            property("direction") = 1
        end
    end

    ex = ex + speed * property("direction") * dt
    setPosition(enemy, ex, ey, ez)
end
```

### 6.3 Guard + attack state machine

This pattern creates a more organic enemy feel.

```lua
properties = {
    state = "patrol",
    speed = 2.0,
    chaseSpeed = 4.2,
    detectionRange = 7.0,
    attackRange = 1.5,
    direction = 1
}

function update(dt)
    local enemy = object("EnemyGuard")
    local player = object("Player")
    if not enemy or not player then return end

    local ex, ey, ez = position(enemy)
    local px, py, pz = position(player)
    local dx = px - ex
    local dist = math.abs(dx)

    if dist < property("detectionRange") then
        property("state") = "chase"
    else
        property("state") = "patrol"
    end

    if property("state") == "patrol" then
        ex = ex + property("speed") * property("direction") * dt
        if ex > 8 then property("direction") = -1 end
        if ex < -8 then property("direction") = 1 end
    elseif property("state") == "chase" then
        if dx < 0 then property("direction") = -1 else property("direction") = 1 end
        ex = ex + property("chaseSpeed") * property("direction") * dt
    end

    if dist < property("attackRange") then
        setShared("playerHealth", (shared("playerHealth") or 3) - 1)
    end

    setPosition(enemy, ex, ey, ez)
end
```

### 6.4 Jumping enemy or ambush enemy

```lua
properties = {
    jumpCooldown = 2.0,
    jumpForce = 7.0,
    currentCooldown = 0.0,
    direction = 1,
    speed = 1.5
}

function update(dt)
    local enemy = object("EnemyJump")
    local player = object("Player")
    if not enemy or not player then return end

    local ex, ey, ez = position(enemy)
    local px, py, pz = position(player)
    local dx = px - ex

    property("currentCooldown") = property("currentCooldown") - dt
    if property("currentCooldown") <= 0 then
        if math.abs(dx) < 6 then
            setShared("enemyVelY", property("jumpForce"))
            property("currentCooldown") = property("jumpCooldown")
        end
    end

    local vy = shared("enemyVelY") or 0
    ey = ey + vy * dt
    if ey < 0 then
        ey = 0
        vy = 0
    end

    setShared("enemyVelY", vy)
    setPosition(enemy, ex + property("speed") * property("direction") * dt, ey, ez)
end
```

### 6.5 AI challenge patterns to reuse

These are the most useful patterns:

- patrol only: simple enemy placement
- patrol + detect player: good for platforming levels
- chase: good for close combat or chase scenes
- ambush: waits until player arrives
- turret: fires or attacks on range
- bounce attack: enemy jumps toward player
- boss pattern: phases, repeated attacks, predictable tells

---

## 7) Player combat and stomp pattern

A Crash-like hero often uses stomp attacks instead of deep melee combat.

```lua
properties = {
    stompForce = 8.0,
    stompCooldown = 0.25,
    attackRange = 2.0,
    attackReady = true
}

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local enemy = object("EnemyPatrol")
    if enemy then
        local ex, ey, ez = position(enemy)
        local dx = ex - x
        local dy = ey - y
        local dist = math.sqrt(dx * dx + dy * dy)

        if dist < property("attackRange") and keyDown("f") then
            setPosition(enemy, ex, ey + 2, ez)
            setShared("enemyDefeated", true)
            sound("sounds/hit.wav", 0.8, 1.2, false)
        end
    end
end
```

### Stomp pattern

The classic pattern is:

- player drops onto enemy from above
- enemy gets hurt or removed
- player gets a small bounce
- enemy corpse or respawn is managed

This is easier than full combat and works very well for Crash-style game flow.

---

## 8) Debugging and playtesting in the editor

The editor workflow is essential. You should debug the game often while it is still small.

### Use logs

```lua
print("Player started")
info("Player health = " .. tostring(shared("playerHealth") or 3))
warning("Jumping too high")
debug("Grounded = " .. tostring(shared("playerOnGround")))
error("No Player object assigned")
```

### Good debugging habits

1. Keep one script small and responsible.
2. Print only the values you need.
3. Check if object names match exactly.
4. Validate `object("Name")` before using it.
5. Test movement in small isolated scenes.
6. Use `setShared()` to debug values between scripts.

Example of safe object lookup:

```lua
local player = object("Player")
if not player then
    warning("Player object not found")
    return
end
```

### Test loop

Every time you change a mechanic:

- press play
- test normal run
- test jump and landing
- test enemy collision
- test respawn and fail state
- test camera follow
- test one level section in isolation

This is the fastest path to stable movement.

---

## 9) Script organization patterns for larger games

For a real project, keep logic separated by responsibility:

- `PlayerController.lua` -> movement, jump, state
- `CameraFollow.lua` -> camera
- `EnemyPatrol.lua` -> patrol logic
- `EnemyChaser.lua` -> chase logic
- `PickupCoin.lua` -> collectibles
- `HazardTrigger.lua` -> death triggers
- `LevelManager.lua` -> score, lives, checkpoints, win state

Example manager script:

```lua
properties = {
    score = 0,
    lives = 3,
    win = false
}

function start()
    setShared("score", 0)
    setShared("playerHealth", 3)
end

function update(dt)
    local score = shared("score") or 0
    local health = shared("playerHealth") or 3

    if health <= 0 then
        info("Player lost all lives")
        setShared("gameOver", true)
    end

    if score >= 100 then
        property("win") = true
        info("Level clear")
    end
end
```

This keeps scripts readable and makes debugging much easier.

---

## 10) Full example: a simple Crash-style level loop

This is a complete mini example for how the game loop can look.

```lua
properties = {
    speed = 6.5,
    jumpForce = 9.5,
    gravity = 25.0,
    groundY = 0.0,
    health = 3,
    moveX = 0.0,
    velY = 0.0,
    respawnX = 0.0,
    respawnY = 2.0,
    respawnZ = 0.0
}

function start()
    setShared("playerHealth", property("health"))
    setShared("score", 0)
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local dx = 0

    if key("a") or key("left") then dx = dx - 1 end
    if key("d") or key("right") then dx = dx + 1 end

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

    x = x + dx * property("speed") * dt

    if moveX ~= 0 then
        if dx > 0 then setRotation(player, 0, 0, 0) end
        if dx < 0 then setRotation(player, 0, 180, 0) end
    end

    setPosition(player, x, y, z)

    if y < -10 then
        setPosition(player, property("respawnX"), property("respawnY"), property("respawnZ"))
        setShared("playerHealth", (shared("playerHealth") or 3) - 1)
    end
end
```

This is the skeleton of a solid platforming prototype.

---

## 11) Deploying the game

When the prototype is working, the next step is deployment.

### Build and test in the editor

1. Open the project in Whisk3D.
2. Create a small test level with:
   - start platform
   - gaps
   - enemy patrols
   - collectible pickups
   - respawn point
3. Press play and test the level.
4. Iterate until movement feels right.

### Export or deploy

The project root includes platform build instructions and platform folders, as described in [README.md](README.md). In practice, the workflow is:

1. Make sure dependencies are initialized:

```bash
git submodule update --init --recursive
```

2. Use the platform-specific build step for your target.
3. Build for desktop, Android, web, or the platform you want.
4. Test on the real target device.
5. Tune input and camera values based on hardware performance.

The repo already includes platform folders such as:

- `platform/windows`
- `platform/linux`
- `platform/mac`
- `platform/android`
- `platform/web`

Use those as your deployment targets rather than improvising your own build flow.

---

## 12) Production advice for a Crash-style prototype

To turn a small playable prototype into a strong game, focus on these five things:

1. Movement feel: run speed, acceleration, jump arc, and falling feel.
2. Readability: the player should always understand what is safe and dangerous.
3. Enemy rhythm: enemies should repeat patterns the player can learn.
4. Level flow: each section should teach one mechanic and then challenge it.
5. Iteration: tune values in small groups and test often.

### Good challenge pacing

A level should feel like this:

- easy movement warm-up
- one gap or enemy introduction
- small obstacle challenge
- reward pickup or checkpoint
- next skill challenge
- repeat until level break

This is the same logic used by many platformer games, including Crash-like design.

---

## 13) Recommended first project checklist

Build this as your first playable prototype:

- [ ] Player object with running and jumping
- [ ] Ground detection and respawn
- [ ] Camera follows player
- [ ] 1 patrol enemy
- [ ] 1 collectible
- [ ] 1 hazard trap
- [ ] 1 checkpoint
- [ ] Playtest loop and polish

Once that works, add:

- double jump or slide
- enemy stomp logic
- moving platforms
- camera smoothing
- win condition
- score and lives

---

## 14) Final advice

The fastest way to make a good Crash-style game in Whisk3D is to keep the scope tight and make the movement feel excellent before adding complexity.

Start simple:

- one player
- one ground plane
- one jumping test
- one enemy patrol
- one collectible
- one camera follow

Then add more systems only when the basics are already satisfying.

If you do this in small steps, the game will feel much better than if you try to write a full combat system, camera system, and enemy AI all at once.

---

## 15) Copy-paste starter script

This is the simplest working platformer starter you can drop into a player object:

```lua
properties = {
    speed = 6.0,
    jumpForce = 9.0,
    gravity = 22.0,
    groundY = 0.0,
    velY = 0.0,
    respawnX = 0.0,
    respawnY = 2.0,
    respawnZ = 0.0
}

function start()
    print("Game started")
end

function update(dt)
    local player = object("Player")
    if not player then
        warning("Player object not found")
        return
    end

    local x, y, z = position(player)
    local move = 0.0

    if key("a") or key("left") then move = move - 1 end
    if key("d") or key("right") then move = move + 1 end

    if keyDown("space") and y <= property("groundY") then
        property("velY") = property("jumpForce")
    end

    property("velY") = property("velY") - property("gravity") * dt
    y = y + property("velY") * dt

    if y < property("groundY") then
        y = property("groundY")
        property("velY") = 0
    end

    x = x + move * property("speed") * dt
    setPosition(player, x, y, z)

    if y < -10 then
        setPosition(player, property("respawnX"), property("respawnY"), property("respawnZ"))
        setShared("playerHealth", (shared("playerHealth") or 3) - 1)
    end
end
```

This is enough to build a small playable observability prototype and then expand from there.

---

## 16) Quick summary

If you want the shortest possible answer:

- make a `Player` object
- attach a script with `properties`
- read input with `key()` and `keyDown()`
- move with `setPosition()` or `move()`
- use `start()` for setup and `update(dt)` for logic
- use `object("Name")` to reference level objects
- debug with `print()`, `info()`, `warning()`, and `error()`
- test in the editor, then build for target platform

This is the practical workflow for making a Crash-style platformer in Whisk3D.
