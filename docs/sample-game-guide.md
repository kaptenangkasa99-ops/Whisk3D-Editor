# Building a Small Platformer with Whisk3D

This guide brings together the platformer, locomotion, level-template,
animation, and touch-control examples. It uses the shared projects in
[`templates/`](../templates/) and the Lua API currently registered by the
runtime.

## 1. Start with the shared platformer

The quickest way to get a playable scene is to open
[`templates/platformer/platformer.w3d`](../templates/platformer/platformer.w3d)
and press **Play**. The project already contains a player, ground, and
touch-friendly controls. Move with **A/D** or the arrow keys and jump with
**Space**. On touch devices, use the on-screen buttons.

The controller is in
[`platformer.lua`](../templates/platformer/platformer.lua). It demonstrates
the project's Y-up coordinates, frame-time movement, gravity, jumping, and
keyboard/touch input. Treat this script as the working reference when
extending the sample.

For a new scene, start small:

- `Player`
- `Ground`
- `CameraRig`
- one `EnemyPatrol`
- one `Coin`
- one `Goal`

Use exact object names when looking them up from Lua. The game loop is easiest
to test with one short section before adding more content.

## 2. Script lifecycle and API

Attach a Lua script to an object. The runtime calls `start()` once and
`update(dt)` every frame. `dt` is elapsed time in seconds.

```lua
function start()
    print("Level started")
end

function update(dt)
    if key("d") then
        -- apply continuous movement
    end
    if keyPressed("space") then
        -- trigger a one-time action
    end
end
```

Use `key()` for a held key and `keyPressed()` for the frame it is first
pressed. Find other scene objects with `object("Name")`. The registered
transform functions are case-sensitive: `Position(obj)` and
`SetPosition(obj, x, y, z)`. See the [Lua API reference](../api.md) for the
current list of functions.

Script `properties` can expose values for editing in the inspector; read them
with `property("name")`. Keep changing per-script state in Lua locals, or use
`setShared("key", value)` and `shared("key")` for state shared by scripts.
`property()` is a getter, not an assignment target.

## 3. Locomotion and camera

The platformer template's controller is a good starting point for running and
jumping. It keeps vertical speed and grounded state in Lua locals, combines
keyboard input with the on-screen left, right, and jump buttons, and clamps
the player to the sample ground height.

When adapting it:

- Set the ground height to match the player's origin and the level geometry.
- Tune speed, jump speed, and gravity together; test both ascent and landing.
- Keep input and movement frame-rate independent by multiplying velocity by
  `dt`.
- Reset vertical speed when landing or respawning.
- Keep movement state local to the player script unless another script needs
  to read it.

A camera can follow the player by moving toward a fixed offset. Place and aim
the camera in the editor first, then adjust its position in `update(dt)`:

```lua
local followSpeed = 5
local offsetX, offsetY, offsetZ = -5, 3, 7

function update(dt)
    local camera = object("CameraRig")
    local player = object("Player")
    if not camera or not player then return end

    local px, py, pz = Position(player)
    local cx, cy, cz = Position(camera)
    local blend = math.min(1, followSpeed * dt)

    SetPosition(camera,
        cx + (px + offsetX - cx) * blend,
        cy + (py + offsetY - cy) * blend,
        cz + (pz + offsetZ - cz) * blend)
end
```

The offset depends on the direction the level travels and the camera's
orientation. Keep enough space ahead of the player to see upcoming jumps.

## 4. Simple level behaviors

### Patrol enemy

Attach a patrol script to an enemy named `EnemyPatrol`. This example uses
local state and moves between two X coordinates:

```lua
local speed = 2
local minX, maxX = -5, 5
local direction = 1

function update(dt)
    local enemy = object("EnemyPatrol")
    if not enemy then return end

    local x, y, z = Position(enemy)
    x = x + speed * direction * dt
    if x < minX then
        x = minX
        direction = 1
    elseif x > maxX then
        x = maxX
        direction = -1
    end
    SetPosition(enemy, x, y, z)
end
```

### Collectible

Attach this to a `Coin` object. `isColliding()` checks overlap, and the local
flag prevents the score from increasing more than once:

```lua
local collected = false
local value = 10

function update(dt)
    local player = object("Player")
    local coin = object("Coin")
    if not player or not coin or collected then return end

    if isColliding(player, coin) then
        collected = true
        setVisible(coin, false)
        setShared("score", (shared("score") or 0) + value)
    end
end
```

The collision helper is useful for simple triggers. For more involved
platforming, verify the result in the actual game and avoid applying damage
every frame while two objects remain in contact; add a cooldown or a
one-contact state.

### Moving platform and respawn

For a moving platform, store its starting position and compute its position
from elapsed time (for example, `baseX + math.sin(elapsed * speed) * amplitude`).
Computing from the base prevents the platform from drifting over time.

For a fall/respawn rule, compare the player's Y position with a fail height.
When the player falls below it, restore the saved spawn position and reset the
controller's vertical speed. Checkpoints can replace that saved position when
the player reaches them.

## 5. Animation and touch controls

The shared [`animation` template](../templates/animation/README.md) shows
time-based movement and rotation, with a UI button that toggles spinning.
For object animation clips, the registered `animator(obj, animationId
[, startFrame])` API takes a numeric animation ID; see the [Lua API
reference](../api.md) rather than assuming clip names or speed/loop arguments.

The shared [`controls` template](../templates/controls/README.md) demonstrates
keyboard controls and on-screen buttons. In scripts, `isDown(button)` is for
continuous actions such as movement, while `isPressed(button)` is for a
one-time action such as jumping. The platformer template combines these
button states with keyboard input and supports simultaneous touch controls.
For custom touch interfaces, `finger(index)` and `getUIBox(obj)` are available;
their coordinate details are in the [Lua API reference](../api.md).

## 6. Playtest and extend

After each mechanic change:

1. Press **Play** and test normal movement, jumping, and landing.
2. Check that the camera keeps the next obstacle visible.
3. Test pickups, hazards, and respawn more than once.
4. Verify that object names match the script lookups.
5. Use `print()`, `info()`, `debug()`, and `error()` to diagnose problems.

Build one clear gameplay loop before expanding it: movement warm-up, a jump or
hazard, an enemy, a reward, and a goal. Then tune movement, spacing, and
readability before adding more systems.

To build a standalone game from the command line, see the
[Windows build instructions](../platform/windows/README.md) for the
`--build-game` command and its platform options.
