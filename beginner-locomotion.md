# Beginner guide: basic locomotion in Whisk3D

This is the shortest practical path to getting a player moving in Whisk3D using Lua.

## 1) Create the player object

1. Open the editor.
2. Create a new object for the player.
3. Rename it to `Player`.
4. Add a Lua script to it.
5. Start with this script:

```lua
properties = {
    speed = 6.0,
    jumpForce = 9.0,
    gravity = 22.0,
    groundY = 0.0,
    velY = 0.0
}

function start()
    print("Player started")
end

function update(dt)
    local player = object("Player")
    if not player then
        warning("Player object not found")
        return
    end

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
end
```

## 2) What this script does

- `properties` stores editable values in the editor.
- `start()` runs once when play begins.
- `update(dt)` runs every frame.
- `key()` checks held keys.
- `keyDown()` checks a press only once.
- `position()` gets the current position.
- `setPosition()` moves the object.
- the vertical velocity handles jumping and gravity.

## 3) Test the movement

1. Create a ground object.
2. Place the player above the ground.
3. Press play.
4. Move with A/D or left/right.
5. Press Space to jump.

If the player falls through the ground, set the ground height correctly or tune `groundY`.

## 4) Add a camera

Create a `CameraRig` object and attach this script:

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

## 5) Add a simple enemy

Create an enemy object named `EnemyPatrol` and use this:

```lua
properties = {
    speed = 2.0,
    minX = -5.0,
    maxX = 5.0,
    direction = 1
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
end
```

## 6) Add a collectible

Create a coin object named `Coin`:

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

## 7) Debugging tips

Use these when things go wrong:

```lua
print("Player position", x, y, z)
warning("Player object not found")
info("Score: " .. tostring(shared("score") or 0))
```

Common problems:

- wrong object name
- ground height too low or high
- float values not tuned
- forgetting `if not object then return end`

## 8) The beginner rule

Do not add too much at once. A strong beginner prototype should have:

- run and jump
- one enemy patrol
- one pickup
- one camera follow
- one goal or respawn system

Then polish the feel.

## 9) Starter summary

The essential movement loop is:

```lua
if key("d") then
    move = 1
end
if keyDown("space") and grounded then
    velY = jumpForce
end
velY = velY - gravity * dt
y = y + velY * dt
setPosition(player, x, y, z)
```

That is the core of a basic platformer in Whisk3D.
