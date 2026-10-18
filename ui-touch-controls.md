# UI touchpad and keyboard-mapped control sample

This pattern shows how to make a simple touch-friendly movement UI without adding a brand-new engine object type. The idea is to read touch position and convert it into the same movement logic the keyboard input would use.

The sample below is intentionally script-driven so it works with the existing UI object APIs and can be adapted to a project-specific interface.

## Goal

- A virtual pad controls left/right/up/down movement.
- A jump button triggers the same logic as pressing the keyboard button.
- The joystick direction is mapped to WASD-like movement.

## Sample script

```lua
properties = {
    deadZone = 0.2,
    moveX = 0.0,
    moveY = 0.0,
    jumpPressed = false
}

function update(dt)
    local player = object("Player")
    local pad = object("TouchPad")
    local jump = object("JumpButton")

    if not player then return end

    local tx, ty, touching = touch()
    local moveX = 0.0
    local moveY = 0.0

    if pad and touching then
        local px, py, pw, ph = getUIBox(pad)
        local cx = px + pw * 0.5
        local cy = py + ph * 0.5

        local dx = (tx - cx) / (pw * 0.5)
        local dy = (ty - cy) / (ph * 0.5)

        if math.abs(dx) > property("deadZone") then
            moveX = dx
        end
        if math.abs(dy) > property("deadZone") then
            moveY = dy
        end
    end

    property("moveX") = moveX
    property("moveY") = moveY

    local left = (moveX < -property("deadZone")) or key("a") or key("left")
    local right = (moveX > property("deadZone")) or key("d") or key("right")
    local up = (moveY > property("deadZone")) or key("w") or key("up")
    local down = (moveY < -property("deadZone")) or key("s") or key("down")

    if left then
        -- same logic as pressing A / left arrow
        -- move the character left
    end
    if right then
        -- same logic as pressing D / right arrow
        -- move the character right
    end
    if up then
        -- same logic as pressing W / up arrow
    end
    if down then
        -- same logic as pressing S / down arrow
    end

    if jump then
        local pressed = isPressed(jump) or keyPressed("space")
        if pressed then
            property("jumpPressed") = true
            -- same logic as a keyboard jump trigger
        else
            property("jumpPressed") = false
        end
    end
end
```

## Mapping behavior

The input rules are intentionally simple:

- left/right is mapped from the pad X position to `A/D`-style movement
- up/down is mapped from the pad Y position to `W/S`-style movement
- the jump button acts like a keyboard jump trigger

This means the same game logic can be shared across mouse, touch, and keyboard without duplicating movement code.

## Example mapping table

```text
Pad direction        Keyboard equivalent
left                A / left arrow
right               D / right arrow
up                  W / up arrow
down                S / down arrow
jump button         Space
```

## Practical notes

- Use `getUIBox(obj)` to get the element rectangle and calculate where the pointer is inside the virtual pad.
- Use a dead zone so the character does not jitter when the touch is near the center.
- Use `isPressed(obj)` for buttons and `keyPressed()` for the actual keyboard input path.
- Convert movement into the same state variables used by your game logic instead of directly pressing keys in the system.

This pattern keeps the control scheme portable and easy to debug while still giving the project a touch-friendly controller and a button that behaves like a keyboard input trigger.
