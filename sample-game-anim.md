# Sample game animation showcase

This example shows a small game loop that uses the animator API to play a run cycle and transition to a jump or idle state.

The goal is to keep the structure simple and readable:

- `start()` sets up default animation state
- `update(dt)` reads input and chooses the animation
- `animator(obj, clipName, speed, loop)` plays an animation clip by name
- `setPosition()` / `rotation()` handle movement

## Minimal example

```lua
properties = {
    speed = 4.0,
    runSpeed = 6.0,
    jumpForce = 9.5,
    gravity = 22.0,
    velY = 0.0,
    grounded = true,
    facing = 1
}

function start()
    local player = object("Player")
    if player then
        animator(player, "idle", 1.0, true)
    end
end

function update(dt)
    local player = object("Player")
    if not player then return end

    local x, y, z = position(player)
    local move = 0

    if key("a") or key("left") then
        move = move - 1
    end
    if key("d") or key("right") then
        move = move + 1
    end

    if move ~= 0 then
        property("facing") = (move > 0) and 1 or -1
        setRotation(player, 0, (property("facing") > 0) and 0 or 180, 0)
        x = x + move * property("speed") * dt
        animator(player, "run", 1.2, true)
    else
        animator(player, "idle", 1.0, true)
    end

    if keyPressed("space") and property("grounded") then
        property("velY") = property("jumpForce")
        property("grounded") = false
        animator(player, "jump", 1.0, false)
    end

    if not property("grounded") then
        property("velY") = property("velY") - property("gravity") * dt
        y = y + property("velY") * dt

        if y <= 0 then
            y = 0
            property("velY") = 0
            property("grounded") = true
        end
    end

    setPosition(player, x, y, z)
end
```

## How it works

- `animator(player, "idle", 1.0, true)` starts the idle loop.
- `animator(player, "run", 1.2, true)` swaps to the run cycle while the player is moving.
- `animator(player, "jump", 1.0, false)` plays a one-shot jump animation.
- Input is still handled by the regular key bindings, so the animation remains responsive and gameplay-friendly.

## Good practice

- Keep clip names consistent with the exported animation names in the project.
- Use a small state machine for `idle`, `run`, `jump`, `fall`, and `attack`.
- Prefer one-shot clips (`loop = false`) for jump and attack transitions.
- Use `keyPressed()` for state changes and `key()` for continuous movement.

## Typical animation flow

```lua
if move ~= 0 then
    animator(player, "run", 1.2, true)
elseif not property("grounded") then
    animator(player, "fall", 1.0, true)
else
    animator(player, "idle", 1.0, true)
end
```

This pattern keeps the player animation readable and avoids constant restart churn when the state has not changed.
