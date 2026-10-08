# Whisk3D Lua scripting guide

This engine treats Lua like a component system: a `.lua` file can be attached to any object, and that script runs in its own Lua state. In practice, the pattern is very close to Unity's `MonoBehaviour` style:

- A script is attached to an object.
- The script exposes editable values via `properties`.
- `start()` runs once when play starts.
- `update(dt)` runs every frame.
- You access other objects with `object("Name")`.

The runtime binds C++ functions into each script's Lua state. Lua names are case-sensitive; use the exact spelling shown in this guide. A script can use the standard Lua base, `math`, `string`, and `table` libraries, plus the Whisk3D functions below. The game/UI bindings are registered alongside the Core API in the editor and game runtime.

---

## 1) Script lifecycle

Every script can define these globals:

```lua
properties = {
    speed = 6,
    jump = 10,
    label = "player",
    active = true,
    difficulty = { "easy", "normal", "hard" },
}

function start()
    -- runs once when the game starts
end

function update(dt)
    -- runs every frame; dt is seconds
end
```

What this means in Unity terms:

- `start()` ~= `Start()`
- `update(dt)` ~= `Update(float dt)`
- `properties` ~= serialized fields / inspector values

You can read instance values with:

```lua
local speed = property("speed")
local mode = option("difficulty")
local ref = object("ball")
```

If a value is not assigned for that instance, the script default from `properties` is used. `property()` reads values; use Lua locals or `shared()` for changing runtime state.

---

## 2) What functions are actually exposed to Lua

These are the actual global functions registered in the engine. They are not theoretical; they are bound at runtime.

### 2.1 Core / object access

```lua
object("ObjectName")
option("optionName")
property("propertyName")
parameter("propertyName", default)
```

Purpose:

- `object()` first gets an editor-assigned object reference by property name, then searches the current scene by exact object name.
- `object("Self")` returns the object the script is attached to.
- `option()` reads a dropdown value chosen in the editor.
- `property()` reads a custom numeric/bool/string value declared in `properties`.
- `parameter()` is the numeric alias of `property()`.

#### Example

```lua
local ball = object("Ball")
if ball then
    local x, y, z = Position(ball)
    print("Ball:", x, y, z)
end
```

---

### 2.2 Transform / object movement

```lua
Position(obj)
SetPosition(obj, x, y, z)
mover(obj, dx, dy, dz)
rotation(obj)
setRotation(obj, x, y, z)
rotate(obj, x, y, z)
scaleObject(obj)
setScale(obj, sx, sy, sz)
scale(obj, factor)
visible(obj)
setVisible(obj, true_or_false)
```

These are the main transform calls.

- `SetPosition(...)` is absolute positioning.
- `mover(...)` moves relative to the current transform.
- `setRotation(...)` sets absolute rotation in degrees.
- `rotate(...)` rotates relative to current rotation.
- `scaleObject(obj)` reads the scale; `scale(obj, factor)` applies a uniform relative scale.

Unity equivalents:

- `SetPosition(obj, x, y, z)` ~= `transform.position = Vector3(x, y, z)`
- `mover(obj, dx, dy, dz)` ~= `transform.Translate(dx, dy, dz)`
- `setRotation(obj, x, y, z)` ~= `transform.eulerAngles = Vector3(x, y, z)`
- `setVisible(obj, true)` ~= `SetActive(true)` or `Renderer.enabled = true`

---

### 2.3 Input

```lua
key("w")
keyPressed("w")
buttonPressed("a")
stick("izq")
stick("der")
touch()
mouse()
finger(1)
button("a")
```

Common uses:

```lua
if key("d") then
    -- move right
end

if keyPressed("space") then
    -- jump once
end

local x, y, active = touch()
local x, y = stick("izq")
local pressed = button("a")
```

Unity equivalents:

- `key("w")` ~= `Input.GetKey("w")`
- `keyPressed("space")` ~= `Input.GetKeyDown(KeyCode.Space)`
- `stick("izq")` ~= `Gamepad left stick vector`
- `button("a")` ~= `Input.GetButton("A")`

---

### 2.4 Audio and sound

```lua
sound("sounds/bip.wav", volume, pitch, loop)
stopSound(handle, fadeSec)
beep(frequency, ms, volume)
```

Examples:

```lua
sound("sounds/coin.wav", 0.7, 1.0, false)
beep(440, 120, 0.4)
```

Unity equivalents:

- `sound(...)` ~= `AudioSource.PlayOneShot(...)`
- `beep(...)` ~= a lightweight synthesized sfx tone

---

### 2.5 Shared state between scripts

```lua
shared("score")
setShared("score", 42)
```

This is the engine's global cross-script data channel.

Example:

```lua
setShared("score", (shared("score") or 0) + 1)
```

Unity equivalent:

- `setShared(...)` ~= static singleton / static variable / shared state manager
- `shared(...)` ~= global static value accessed from multiple scripts

---

### 2.6 Config and logging

```lua
config("key", "defaultValue")
setConfig("key", "value")
saveConfig()
loadConfig()
silence()
isMuted()
info("message")
logInfo("message")
error("message")
debug("message")
print("hello")
```

Unity equivalents:

- `config(...)` ~= `PlayerPrefs.GetString(...)`
- `setConfig(...)` ~= `PlayerPrefs.SetString(...)`
- `info(...)` ~= `Debug.Log(...)`
- `logInfo(...)` ~= `Debug.LogWarning(...)`
- `error(...)` ~= `Debug.LogError(...)`

---

### 2.7 UI / 2D game objects

These are bound by the game layer:

```lua
screen()
posPx(obj)
setPosPx(obj, x, y)
getScreenPx(obj)
setScreenPx(obj, w, h)
setText(obj, text)
setTextura(obj, "path/image.png")
show(obj, true)
setOpacity(obj, 1.0)
setFontScreenSize(obj, size)
```

Use these for HUD, buttons, health bars, menus, and screen-space UI.

Unity equivalent:

- `screen()` ~= `Screen.width`, `Screen.height`
- `setText(obj, text)` ~= `Text.text = ...`
- `setTextura(obj, ...)` ~= `Image.sprite = ...`

---

### 2.8 Camera / viewport helpers

```lua
screenOf(obj)
setRail(obj, curve, node)
setRailNode(obj, value)
setRailLookAt(obj, true)
railOf(obj)
lensOf(obj)
setLens(obj, fov, near, far)
cameraXZ()
target()
parameter("name", default)
```

These are the camera/gameplay helpers for moving along curves, setting lenses, and using camera-relative motion.

Unity equivalents:

- `cameraXZ()` ~= camera-forward/right plane vectors
- `target()` ~= a player target or follow target reference
- `setRail(...)` ~= path-following camera controller

---

## 3) Minimal game sample

This is a tiny player controller using the actual functions exposed by the engine.

```lua
properties = {
    speed = 6.0,
    jump = 12.0,
    acceleration = 14.0,
}

local player = nil
local yBase = 0
local jumpActive = false

function start()
    player = object("Player")
    if not player then
        error("Player object not found")
        return
    end

    local x, y, z = Position(player)
    yBase = y
    setShared("points", 0)
    info("Game ready")
end

function update(dt)
    if not player then return end

    local vx = 0.0
    local vy = 0.0

    if key("a") or key("left") then
        vx = vx - 1
    end

    if key("d") or key("right") then
        vx = vx + 1
    end

    if keyPressed("space") and not jumpActive then
        jumpActive = true
        vy = property("jump")
    end

    if not key("space") and jumpActive then
        local x, y, z = Position(player)
        if y <= yBase then
            jumpActive = false
            SetPosition(player, x, yBase, z)
        end
    end

    mover(player, vx * property("speed") * dt, vy * dt, 0)

    local x, y, z = Position(player)
    if y < yBase then
        SetPosition(player, x, yBase, z)
        jumpActive = false
    end
end
```

### A more game-like sample: coin collection

```lua
properties = {
    name = "Coin",
    value = 1,
}

local coin = nil
local active = true

function start()
    coin = object("Coin")
    if coin then
        print("Coin loaded")
    end
end

function update(dt)
    if not coin or not active then return end

    local x, y, z = Position(coin)
    local player = object("Player")
    if player then
        local px, py, pz = Position(player)
        local dist = math.sqrt((px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2)

        if dist < 1.5 then
            setShared("points", (shared("points") or 0) + property("value"))
            setVisible(coin, false)
            active = false
            sound("sounds/coin.wav", 0.7, 1.0, false)
        end
    end
end
```

This is the same idea as a Unity `OnTriggerEnter` + score update, but written in Lua with object references and shared state.

---

## 4) Unity C# equivalent mental model

Think of Whisk3D Lua as a lightweight Unity-like scripting system.

| Whisk3D Lua | Unity C# idea |
| --- | --- |
| `properties = { ... }` | `[SerializeField]` / inspector editable fields |
| `function start()` | `Start()` |
| `function update(dt)` | `Update()` |
| `object("Name")` | `GameObject.Find("Name")` |
| `SetPosition(obj, x, y, z)` | `transform.position = new Vector3(x, y, z)` |
| `mover(obj, dx, dy, dz)` | `transform.Translate(dx, dy, dz)` |
| `rotate(obj, x, y, z)` | `transform.Rotate(x, y, z)` |
| `setVisible(obj, true)` | `gameObject.SetActive(true)` or renderer enabled |
| `shared("score")` | static/global state |
| `setShared("score", v)` | `GameManager.Instance.score = v` |
| `key("w")` | `Input.GetKey("w")` |
| `keyPressed("space")` | `Input.GetKeyDown(KeyCode.Space)` |
| `sound("file.wav")` | `AudioSource.PlayOneShot(...)` |
| `info(...)` | `Debug.Log(...)` |
| `error(...)` | `Debug.LogError(...)` |

The main difference is that in Whisk3D the script is attached to a scene object and the engine exposes a direct C++-bound API, while Unity uses C# layer APIs around UnityEngine.

---

## 5) How to make a small game in this project

### Step 1: Add a script to an object

Create or assign a `.lua` file to an object in the editor.

### Step 2: Expose data in the inspector

```lua
properties = {
    speed = 5,
    score = 0,
    name = "Enemy",
    active = true,
}
```

These values appear as editable instance values in the editor.

### Step 3: Get references to other objects

```lua
local player = object("Player")
local enemy = object("Enemy")
```

### Step 4: Move or react each frame

```lua
function update(dt)
    local x, y, z = Position(player)
    if key("d") then
        mover(player, 2 * dt, 0, 0)
    end
end
```

### Step 5: Share state across scripts

```lua
setShared("score", (shared("score") or 0) + 1)
```

This is how multiple objects coordinate without a complex manager object.

### Step 6: Keep logic simple

Use this pattern:

- `start()` for setup and references
- `update(dt)` for movement and checks
- `shared()` for game-wide state
- `object("...")` for object lookup
- one script per object or a few scripts working together

---

## 6) Recommended beginner structure

For a simple game, prefer this layout:

- `Player.lua` — movement, input, collisions
- `Coin.lua` — pickup logic
- `Enemy.lua` — chase / attack logic
- `HUD.lua` — reads `shared("score")` and updates UI text

This looks very much like Unity's `PlayerController`, `Pickup`, `EnemyAI`, and `HUD` scripts.

---

## 7) Quick cheatsheet

```lua
-- create/read instance values
property("speed")
option("difficulty")
object("Ball")

-- transform
SetPosition(obj, x, y, z)
mover(obj, dx, dy, dz)
setRotation(obj, x, y, z)
rotate(obj, x, y, z)
setVisible(obj, true)

-- input
key("w")
keyPressed("space")
stick("izq")
touch()
mouse()

-- audio
sound("sfx.wav")
beep(440, 120, 0.5)

-- state
setShared("score", 10)
shared("score")

-- config
config("music", "on")
setConfig("music", "off")

-- log
info("hello")
logInfo("warning")
error("bad")
```

---

## 8) Final idea

Whisk3D Lua is not a random Lua environment. It is a Unity-like object scripting layer with:

- object references by name
- inspector-like property values
- lifecycle hooks `start()` and `update(dt)`
- shared game state via `shared()`
- direct transforms, audio, UI, and input hooks

If you think in Unity terms, the mental mapping is:

- object script instance = `MonoBehaviour`
- `properties` = serialized fields
- `object("X")` = `GameObject.Find("X")`
- `shared()` = static/global manager state
- `start()` = `Start()`
- `update(dt)` = `Update()`

That is the shortest path to writing a game in this engine.

---

# API Reference

Complete API documentation for all Lua functions exposed by Whisk3D. Functions are organized by category.

## Complete registered API index

These are the Whisk3D globals registered for scripts. Names and capitalization are intentional. The stock Lua base, `math`, `string`, and `table` libraries are also available; `print` is redirected to the engine log, and Whisk3D replaces Lua's standard `error` with its own log function.

| Category | Registered functions |
| --- | --- |
| Object and properties | `object`, `property`, `option`, `parameter`, `type`, `name` |
| Transforms and visibility | `Position`, `SetPosition`, `mover`, `rotation`, `setRotation`, `rotate`, `scaleObject`, `setScale`, `scale`, `visible`, `setVisible`, `pos3`, `setPos3` |
| Input and random | `key`, `keyPressed`, `buttonPressed`, `button`, `stick`, `touch`, `finger`, `mouse`, `random` |
| Shared state and configuration | `shared`, `setShared`, `config`, `setConfig`, `saveConfig`, `loadConfig` |
| Logging and sound | `print`, `info`, `logInfo`, `error`, `debug`, `esDebug`, `logQuantity`, `logLine`, `beep`, `sound`, `stopSound`, `silence`, `isMuted` |
| Lighting and animation | `color`, `setColor`, `energy`, `setEnergy`, `animator`, `rotateTowards`, `instantiate` |
| Vertex editing | `grupoVertices`, `verticePos`, `setVerticePos`, `setVerticeColor`, `setVertices` |
| Physics | `velocidad`, `acelerar`, `caja`, `rebotar`, `rebotarEn`, `rebotarDentro` |
| Screen and UI | `screen`, `posPx`, `setPosPx`, `getScreenPx`, `setScreenPx`, `setText`, `setTextura`, `show`, `setOpacity`, `setFontScreenSize`, `isPressed`, `isColliding`, `clamp`, `isInside`, `getScale`, `getSafeArea`, `fade`, `getUIBox`, `screenOf`, `quot` |
| Camera, visibility, and game | `setRail`, `setRailNode`, `setRailLookAt`, `railOf`, `lensOf`, `setLens`, `cameraXZ`, `target`, `controllers`, `controller`, `setSector`, `visibilityCell`, `setVisibilityAnchor`, `setVisibilityCurve`, `setCollection`, `emitter`, `cambiarEscena` |

The physics functions are still registered in builds made without the physics module, but those builds provide disabled stubs. Some scene, visibility, particle, and collection operations depend on runtime hooks; see their individual descriptions below.

## Object & Script Lifecycle

### `object(name: string) → Object | nil`
Looks up an editor-assigned script reference first, then searches by exact name in the scene containing this script. Passing `"Self"` returns the object that owns the script.

**Parameters:**
- `name`: Object name to search for

**Returns:** Object reference or nil if not found

**Example:**
```lua
local player = object("Player")
if player then
    print("Found player")
end
local selfObject = object("Self")
```

---

### `property(name: string) → number | string | bool`
Reads a property value from the current script's `properties` table.

**Parameters:**
- `name`: Property key name

**Returns:** Property value, or nil if not defined

**Example:**
```lua
function update(dt)
    local speed = property("speed")
    mover(object("Self"), speed * dt, 0, 0)
end
```

---

### `option(name: string) → string`
Reads a dropdown option value from the current script.

**Parameters:**
- `name`: Option key name

**Returns:** Selected option value, or an empty string if no value is assigned

**Example:**
```lua
local difficulty = option("difficulty")
if difficulty == "hard" then
    local enemyCount = 10
    print("Hard mode enemy count:", enemyCount)
end
```

---

### `parameter(name: string, default: number) → number`
Legacy numeric-property helper with a fallback value. In the current runtime implementation it falls back to `default` instead of reading the configured property, so use `property(name)` for a configured value.

**Parameters:**
- `name`: Property key name
- `default`: Default value if not set

**Returns:** Numeric property value

**Example:**
```lua
local health = parameter("health", 100)
```

---

## Transform / Movement

### `Position(obj: Object) → x, y, z`
Gets the object's local position relative to its parent (or its world position when it has no parent).

**Parameters:**
- `obj`: Object to query

**Returns:** Three numbers: x, y, z coordinates

**Example:**
```lua
local x, y, z = Position(player)
print("Player at:", x, y, z)
```

---

### `SetPosition(obj: Object, x: number, y: number, z: number)`
Sets the object's position relative to its parent (or its world position when it has no parent).

**Parameters:**
- `obj`: Object to move
- `x, y, z`: Target coordinates

**Returns:** None

**Example:**
```lua
SetPosition(player, 0, 5, 0)  -- teleport to (0, 5, 0)
```

---

### `mover(obj: Object, dx: number, dy: number, dz: number)`
Moves an object relative to its current position.

**Parameters:**
- `obj`: Object to move
- `dx, dy, dz`: Movement deltas

**Returns:** None

**Example:**
```lua
function update(dt)
    if key("w") then
        mover(player, 0, 0, 5 * dt)  -- move forward
    end
end
```

---

### `rotation(obj: Object) → x, y, z`
Gets the rotation of an object in degrees.

**Parameters:**
- `obj`: Object to query

**Returns:** Three numbers: euler angles x, y, z (in degrees)

**Example:**
```lua
local rx, ry, rz = rotation(player)
```

---

### `setRotation(obj: Object, x: number, y: number, z: number)`
Sets the absolute rotation of an object in degrees.

**Parameters:**
- `obj`: Object to rotate
- `x, y, z`: Target euler angles in degrees

**Returns:** None

**Example:**
```lua
setRotation(enemy, 0, 45, 0)  -- face 45 degrees
```

---

### `rotate(obj: Object, x: number, y: number, z: number)`
Rotates an object relative to its current rotation.

**Parameters:**
- `obj`: Object to rotate
- `x, y, z`: Rotation deltas in degrees

**Returns:** None

**Example:**
```lua
rotate(projectile, 0, 10 * dt, 0)  -- spin around Y axis
```

---

### `scaleObject(obj: Object) → sx, sy, sz`
Gets the object's scale.

**Parameters:**
- `obj`: Object to query

**Returns:** Three numbers: scale x, y, z

**Example:**
```lua
local sx, sy, sz = scaleObject(enemy)
```

---

### `setScale(obj: Object, sx: number [, sy: number [, sz: number]])`
Sets the absolute scale of an object. Omitted axes use `sx`, so one number sets a uniform scale.

**Parameters:**
- `obj`: Object to scale
- `sx`: Target X scale
- `sy, sz`: Optional Y and Z scales (default to `sx`)

**Returns:** None

**Example:**
```lua
setScale(explosion, 2, 2, 2)  -- double size
```

---

### `scale(obj: Object, factor: number)`
Multiplies all three scale axes by the same relative factor.

**Parameters:**
- `obj`: Object to scale
- `factor`: Scale multiplier

**Returns:** None

**Example:**
```lua
scale(ui_button, 1.1)  -- 10% bigger on every axis
```

---

### `visible(obj: Object) → bool`
Checks if an object is visible.

**Parameters:**
- `obj`: Object to check

**Returns:** true if visible, false otherwise

**Example:**
```lua
if visible(coin) then
    print("Coin is visible")
end
```

---

### `setVisible(obj: Object, visible: bool)`
Sets the visibility of an object.

**Parameters:**
- `obj`: Object to modify
- `visible`: true to show, false to hide

**Returns:** None

**Example:**
```lua
setVisible(npc, false)  -- hide the NPC
```

---

### `name(obj: Object) → string`
Gets the name of an object.

**Parameters:**
- `obj`: Object to query

**Returns:** Object name

**Example:**
```lua
local objName = name(player)
```

---

### `type(obj: Object) → string`
Gets the type of an object (e.g., "mesh", "light", "camera", "texto2d").

**Parameters:**
- `obj`: Object to query

**Returns:** Type name as string

**Example:**
```lua
if type(obj) == "mesh" then
    -- it's a 3D mesh
end
```

---

## Input

### `key(name: string) → bool`
Checks if a key is currently pressed.

**Parameters:**
- `name`: Key name ("w", "a", "s", "d", "space", "left", "right", "up", "down", etc.)

**Returns:** true if pressed, false otherwise

**Example:**
```lua
if key("w") then
    mover(player, 0, 0, speed * dt)
end
```

---

### `keyPressed(name: string) → bool`
Checks if a key was just pressed (only true for one frame).

**Parameters:**
- `name`: Key name

**Returns:** true only on the frame the key is pressed, false otherwise

**Example:**
```lua
if keyPressed("space") then
    startJump()
end
```

---

### `buttonPressed(name: string) → bool`
Checks if a gamepad button was just pressed.

**Parameters:**
- `name`: Button name ("a", "b", "x", "y", "start", "select", etc.)

**Returns:** true on the frame pressed, false otherwise

**Example:**
```lua
if buttonPressed("a") then
    attack()
end
```

---

### `button(name: string) → bool`
Checks if a gamepad button is currently pressed.

**Parameters:**
- `name`: Button name

**Returns:** true if pressed, false otherwise

**Example:**
```lua
if button("lb") then
    -- trigger action
end
```

---

### `stick(side: string) → x, y`
Gets the analog stick position.

**Parameters:**
- `side`: `"izq"` (left, default) or `"der"` (right)

**Returns:** Two numbers: x and y in range [-1, 1]

**Example:**
```lua
local lx, ly = stick("izq")
local rx, ry = stick("der")
mover(player, lx * speed * dt, 0, ly * speed * dt)
```

---

### `touch() → x, y, active`
Gets the primary touch position and state in canvas coordinates (centered at 0, 0).

**Parameters:** None

**Returns:** Three values: x, y (canvas coordinates), active (true if touching)

**Example:**
```lua
local x, y, active = touch()
if active then
    print("Touch at:", x, y)
end
```

---

### `finger(index: number) → x, y, active`
Gets the state of a specific touch finger (multitouch).

**Parameters:**
- `index`: Finger index (0-3)

**Returns:** Three values: x, y (canvas coordinates), active (true if touching)

**Example:**
```lua
local x, y, active = finger(0)
```

---

### `mouse() → x, y`
Gets the mouse cursor position in canvas coordinates (centered at 0, 0).

**Parameters:** None

**Returns:** Two numbers: x, y in canvas coordinates

**Example:**
```lua
local mx, my = mouse()
```

---

## Audio

### `sound(path: string, volume: number, pitch: number, loop: bool) → handle | nil`
Plays a sound effect.

**Parameters:**
- `path`: Path to sound file (e.g., "sounds/coin.wav")
- `volume`: Optional volume (0.0 to 1.0, default 1.0)
- `pitch`: Optional pitch multiplier (1.0 = normal, default 1.0)
- `loop`: Optional boolean; true to loop, false to play once

**Returns:** Handle to stop the sound later, or `nil` if playback could not start

**Example:**
```lua
local sfx = sound("sounds/coin.wav", 0.7, 1.0, false)
if sfx then stopSound(sfx) end
```

---

### `stopSound(handle: number, fadeSec: number)`
Stops a currently playing sound.

**Parameters:**
- `handle`: Sound handle from `sound()` call
- `fadeSec`: Fade duration in seconds (0 = instant)

**Returns:** None

**Example:**
```lua
stopSound(music_handle, 2.0)  -- fade out over 2 seconds
```

---

### `beep(frequency: number, ms: number, volume: number)`
Plays a synthesized tone.

**Parameters:**
- `frequency`: Frequency in Hz (e.g., 440 = A4)
- `ms`: Duration in milliseconds
- `volume`: Volume (0.0 to 1.0)

**Returns:** None

**Example:**
```lua
beep(440, 100, 0.5)  -- A4 note for 100ms
```

---

## Shared State & Config

### `shared(key: string) → value`
Reads a global value shared across all scripts.

**Parameters:**
- `key`: Key name

**Returns:** Stored value or nil if not set

**Example:**
```lua
local score = shared("score") or 0
```

---

### `setShared(key: string, value: any)`
Sets a global value accessible to all scripts.

**Parameters:**
- `key`: Key name
- `value`: Value to store (number, string, bool, etc.)

**Returns:** None

**Example:**
```lua
setShared("score", shared("score") + 10)
```

---

### `config(key: string, default: string) → string`
Reads a persistent config value (saved to disk).

**Parameters:**
- `key`: Config key
- `default`: Default value if not set

**Returns:** Config value

**Example:**
```lua
local musicVolume = config("musicVolume", "0.8")
```

---

### `setConfig(key: string, value: string)`
Sets a persistent config value.

**Parameters:**
- `key`: Config key
- `value`: Value to store (stored as string)

**Returns:** None

**Example:**
```lua
setConfig("difficulty", "hard")
```

---

### `saveConfig()`
Saves all config changes to disk.

**Parameters:** None

**Returns:** None

**Example:**
```lua
setConfig("lastLevel", "5")
saveConfig()
```

---

### `loadConfig()`
Loads config from disk.

**Parameters:** None

**Returns:** None

**Example:**
```lua
loadConfig()
local level = config("lastLevel", "1")
```

---

### `silence()`
Globally mutes all sound.

**Parameters:** None

**Returns:** None

**Example:**
```lua
silence()
```

---

### `isMuted() → bool`
Checks if audio is muted.

**Parameters:** None

**Returns:** true if muted, false otherwise

**Example:**
```lua
if not isMuted() then
    sound("sounds/sfx.wav", 0.5, 1.0, false)
end
```

---

## Logging

### `info(message: string)`
Prints an info message to the console.

**Parameters:**
- `message`: Message text

**Returns:** None

**Example:**
```lua
info("Player spawned at x=" .. x)
```

---

### `logInfo(message: string)`
Writes a warning-level message to the engine log.

**Parameters:**
- `message`: Message text

**Returns:** None

**Example:**
```lua
logInfo("Low on ammo!")
```

---

### `error(message: string)`
Writes an error message to the engine log. This replaces Lua's standard `error()` and does not stop the script; use `assert()` to abort the current callback.

**Parameters:**
- `message`: Error text

**Returns:** None

**Example:**
```lua
if not player then
    error("Player not found!")
end
```

---

### `debug(message: string)`
Prints a debug message (only shown if debug mode is enabled).

**Parameters:**
- `message`: Debug text

**Returns:** None

**Example:**
```lua
debug("Frame " .. frameCount .. ": " .. x .. ", " .. y)
```

---

### `esDebug() → bool`
Checks if the engine is running in debug mode.

**Parameters:** None

**Returns:** true if debug mode, false otherwise

**Example:**
```lua
if esDebug() then
    debug("Detailed debug info")
end
```

---

### `print(message: string)`
Prints a message (standard Lua print, redirected to the Whisk3D console).

**Parameters:**
- `message`: Message text

**Returns:** None

**Example:**
```lua
print("x=" .. x .. ", y=" .. y)
```

---

## 2D Screen / UI

### `screen() → width, height`
Gets the screen resolution.

**Parameters:** None

**Returns:** Two numbers: width and height in pixels

**Example:**
```lua
local w, h = screen()
print("Screen: " .. w .. "x" .. h)
```

---

### `posPx(obj: Object) → x, y`
Gets the 2D screen position of a UI element (in screen pixels).

**Parameters:**
- `obj`: UI object

**Returns:** Two numbers: x, y in screen coordinates

**Example:**
```lua
local x, y = posPx(button)
```

---

### `setPosPx(obj: Object, x: number | nil, y: number | nil)`
Sets the 2D screen position of a UI element. Pass nil to leave an axis unchanged.

**Parameters:**
- `obj`: UI object
- `x`: X position (nil to keep current)
- `y`: Y position (nil to keep current)

**Returns:** None

**Example:**
```lua
setPosPx(button, 100, nil)  -- move right only
```

---

### `getScreenPx(obj: Object) → width, height`
Gets the 2D size of a UI element in screen pixels.

**Parameters:**
- `obj`: UI object

**Returns:** Two numbers: width, height

**Example:**
```lua
local w, h = getScreenPx(panel)
```

---

### `setScreenPx(obj: Object, width: number, height: number)`
Sets the 2D size of a UI element in screen pixels.

**Parameters:**
- `obj`: UI object
- `width`: Width in pixels
- `height`: Height in pixels

**Returns:** None

**Example:**
```lua
setScreenPx(panel, 256, 128)
```

---

### `setText(obj: Object, text: string)`
Sets the text of a 2D text object.

**Parameters:**
- `obj`: Text2D object
- `text`: Text content

**Returns:** None

**Example:**
```lua
setText(scoreLabel, "Score: " .. score)
```

---

### `setTextura(obj: Object, path: string)`
Sets the texture/image of a 2D image object.

**Parameters:**
- `obj`: Image2D or UI element
- `path`: Path to image file

**Returns:** None

**Example:**
```lua
setTextura(healthBar, "images/health_full.png")
```

---

### `show(obj: Object, visible: bool)`
Shows or hides a 2D UI element (alias for `setVisible`).

**Parameters:**
- `obj`: UI object
- `visible`: true to show, false to hide

**Returns:** None

**Example:**
```lua
show(pauseMenu, true)
```

---

### `setOpacity(obj: Object, opacity: number)`
Sets the transparency of a 2D element.

**Parameters:**
- `obj`: UI object
- `opacity`: Opacity from 0.0 (transparent) to 1.0 (opaque)

**Returns:** None

**Example:**
```lua
setOpacity(fadeOverlay, 0.5)
```

---

### `setFontScreenSize(obj: Object, size: number)`
Sets the screen-space font size of a 2D text object.

**Parameters:**
- `obj`: Text2D object
- `size`: Font size in screen pixels

**Returns:** None

**Example:**
```lua
setFontScreenSize(title, 48)
```

---

### `getUIBox(obj: Object) → x, y, width, height`
Gets the screen-space bounding box of a UI element (after layout resolution).

**Parameters:**
- `obj`: UI object

**Returns:** Four numbers: center x, center y, width, height

**Example:**
```lua
local cx, cy, w, h = getUIBox(button)
```

---

### `screenOf(obj: Object) → x, y, inFront`
Projects a 3D object's position to 2D screen coordinates.

**Parameters:**
- `obj`: 3D object to project

**Returns:** Three values: screen x, screen y, inFront (bool - true if in front of camera)

**Example:**
```lua
local sx, sy, inFront = screenOf(enemy)
if inFront then
    setPosPx(enemyIndicator, sx, sy)
end
```

---

## 2D Collision & Interaction

### `isColliding(a: Object, b: Object) → bool`
Checks if two 2D screen objects overlap.

**Parameters:**
- `a`: First object
- `b`: Second object

**Returns:** true if overlapping, false otherwise

**Example:**
```lua
if isColliding(player, coin) then
    collectCoin()
end
```

---

### `clamp(obj: Object, area: Object [, axis: string])` / `clamp(obj, min, max [, axis])`
Constrains a 2D object to stay within an area object or the supplied canvas-coordinate bounds.

**Parameters:**
- `obj`: Object to constrain
- `area`: Area object, or numeric minimum and maximum bounds
- `axis`: Optional axis selector (`"x"`, `"y"`, or `"xy"`; defaults to both)

**Returns:** None

**Example:**
```lua
clamp(ball, bounds)  -- keep ball inside bounds
clamp(paddle, bounds, "y")  -- only constrain Y axis
clamp(paddle, -200, 200, "x") -- constrain to numeric X bounds
```

---

### `isInside(area: Object, x: number, y: number) → bool`
Checks if a 2D point is inside an area's bounding box.

**Parameters:**
- `area`: Area/collision object
- `x, y`: Point coordinates

**Returns:** true if point is inside, false otherwise

**Example:**
```lua
local mx, my = mouse()
if isInside(button, mx, my) then
    print("Mouse over button")
end
```

---

### `isPressed(obj: Object) → bool`
Checks if a UI button/area was just tapped/clicked this frame.

**Parameters:**
- `obj`: Button or UI object

**Returns:** true only on the frame of the tap/click, false otherwise

**Example:**
```lua
if isPressed(playButton) then
    startGame()
end
```

---

### `fade(obj: Object, target: number, step: number)`
Smoothly fades an object's opacity toward a target value.

**Parameters:**
- `obj`: Object to fade
- `target`: Target opacity (0.0 to 1.0)
- `step`: Max change per frame

**Returns:** None

**Example:**
```lua
fade(fadeOverlay, 1.0, 0.01)  -- fade to black
```

---

### `getScale() → factor`
Gets the responsive scale factor (min(width, height) / 480).

**Parameters:** None

**Returns:** Scale factor for responsive UI sizing

**Example:**
```lua
local scale = getScale()
local scaledSize = 100 * scale
```

---

### `getSafeArea() → x, y, width, height`
Gets the safe area (avoiding notches/UI bars) in screen coordinates.

**Parameters:** None

**Returns:** Four numbers: safe area rect

**Example:**
```lua
local sx, sy, sw, sh = getSafeArea()
```

---

## Animation & Camera

### `animator(obj: Object, animation: number [, startFrame: number])`
Selects a vertex animation by its numeric index. With two arguments it queues the animation after the current clip; an optional start frame changes to it immediately.

**Parameters:**
- `obj`: Object with animation
- `animation`: Animation index
- `startFrame`: Optional frame at which to start immediately

**Returns:** None

**Example:**
```lua
animator(player, 1)
animator(enemy, 2, 0)
```

---

### `rotateTowards(obj: Object, dx: number, dz: number [, factor: number])`
Smoothly rotates an object toward a direction on the XZ plane.

**Parameters:**
- `obj`: Object to rotate
- `dx, dz`: Direction vector on the XZ plane
- `factor`: Optional interpolation amount (default `0.2`)

**Returns:** None

**Example:**
```lua
local px, py, pz = Position(player)
local ex, ey, ez = Position(enemy)
rotateTowards(enemy, px - ex, pz - ez, 0.2)
```

---

### `setRail(obj: Object, curve: Object | string, nodeOffset: number)`
Makes a camera follow a bezier curve path.

**Parameters:**
- `obj`: Camera object
- `curve`: Curve object or curve name
- `nodeOffset`: Starting node (optional)

**Returns:** None

**Example:**
```lua
setRail(camera, "railPath")
```

---

### `setRailNode(obj: Object, value: number)`
Sets the current position along a rail path (0 to 1).

**Parameters:**
- `obj`: Camera on a rail
- `value`: Position from 0 (start) to 1 (end)

**Returns:** None

**Example:**
```lua
setRailNode(camera, 0.5)  -- halfway along the path
```

---

### `setRailLookAt(obj: Object, enabled: bool)`
Enables/disables the camera looking along the rail direction.

**Parameters:**
- `obj`: Camera
- `enabled`: true to look along rail, false to ignore

**Returns:** None

**Example:**
```lua
setRailLookAt(camera, true)
```

---

### `railOf(obj: Object) → name`
Gets the name of the rail curve a camera is following.

**Parameters:**
- `obj`: Camera

**Returns:** Rail curve name

**Example:**
```lua
local railName = railOf(camera)
```

---

### `lensOf(obj: Object) → fov, near, far`
Gets the camera lens properties.

**Parameters:**
- `obj`: Camera object

**Returns:** Three numbers: field of view, near plane, far plane

**Example:**
```lua
local fov, near, far = lensOf(camera)
```

---

### `setLens(obj: Object, fov: number, near: number, far: number)`
Sets the camera lens properties.

**Parameters:**
- `obj`: Camera object
- `fov`: Field of view angle
- `near`: Near clipping plane
- `far`: Far clipping plane

**Returns:** None

**Example:**
```lua
setLens(camera, 60, 0.1, 1000)
```

---

### `cameraXZ() → forwardX, forwardZ, rightX, rightZ`
Gets camera-relative direction vectors (ignoring Y).

**Parameters:** None

**Returns:** Four numbers: forward x, forward z, right x, right z

**Example:**
```lua
local fx, fz, rx, rz = cameraXZ()
mover(player, rx * speed * dt, 0, fz * speed * dt)
```

---

### `target() → obj`
Gets the target object the current script is following.

**Parameters:** None

**Returns:** Target object or nil

**Example:**
```lua
local follow = target()
```

---

## Misc / Game Control

### `random() → number` / `random(min: integer, max: integer) → integer`
Returns a random float in `[0, 1)`, or an integer in the inclusive range when both bounds are provided.

**Parameters:** Optional integer minimum and maximum (both required together)

**Returns:** Random float `[0, 1)` or inclusive-range integer

**Example:**
```lua
if random() < 0.5 then
    jumpLeft()
else
    jumpRight()
end
```

---

### `instantiate(prefab: string [, x: number, y: number, z: number]) → Object | nil`
Creates an instance of a prefab from the project's prefab library.

**Parameters:**
- `prefab`: Prefab name
- `x, y, z`: Optional position in engine coordinates

**Returns:** New object reference, or `nil` if the prefab is unavailable

**Example:**
```lua
local newBullet = instantiate("Bullet", 0, 1, 0)
```

---

### `quot()`
Requests the game to close.

**Parameters:** None

**Returns:** None

**Example:**
```lua
if keyPressed("escape") then
    quot()
end
```

---

### `emitter(obj: Object, count: number)`
Emits particles from a particle emitter object.

**Parameters:**
- `obj`: Particle emitter object
- `count`: Number of particles to emit

**Returns:** None

**Example:**
```lua
emitter(dustEmitter, 10)
```

---

## Controller / Input Devices

### `controllers() → count`
Gets the number of connected game controllers.

**Parameters:** None

**Returns:** Number of controllers (0-4)

**Example:**
```lua
if controllers() > 0 then
    print("Gamepad detected")
end
```

---

### `controller(index: number) → name, type`
Gets the name and type of a connected controller. The index is 1-based.

**Parameters:**
- `index`: Controller index (1-based)

**Returns:** Controller name and type strings, or `nil` if that index is not connected

**Example:**
```lua
for i=1, controllers() do
    local controllerName, controllerType = controller(i)
    print("Controller " .. i .. ": " .. controllerName .. " (" .. controllerType .. ")")
end
```

---

## Color & Lighting

### `color(obj: Object) → r, g, b`
Gets the normalized tone of a light; light brightness is read separately with `energy()`.

**Parameters:**
- `obj`: Object (light, material, etc.)

**Returns:** Three numbers: r, g, b (0.0 to 1.0); returns zeros for non-light objects

**Example:**
```lua
local r, g, b, a = color(light)
```

---

### `setColor(obj: Object, r: number, g: number, b: number)`
Sets a light's normalized tone while retaining its brightness.

**Parameters:**
- `obj`: Object
- `r, g, b`: Color components (0.0 to 1.0)

**Returns:** None

**Example:**
```lua
setColor(light, 1.0, 0.5, 0.0)  -- orange
```

---

### `energy(obj: Object) → value`
Gets the brightness/energy of a light.

**Parameters:**
- `obj`: Light object

**Returns:** Energy value

**Example:**
```lua
local brightness = energy(light)
```

---

### `setEnergy(obj: Object, value: number)`
Sets the brightness/energy of a light.

**Parameters:**
- `obj`: Light object
- `value`: Energy multiplier

**Returns:** None

**Example:**
```lua
setEnergy(light, 0.5)  -- dim the light
```

---

## Vertex Manipulation (Advanced)

### `grupoVertices(obj: Object, group: string) → indices`
Gets the list of vertex groups in a mesh.

**Parameters:**
- `obj`: Mesh object
- `group`: Vertex group name

**Returns:** Array of 1-based vertex indices in the named group (empty if missing)

**Example:**
```lua
local indices = grupoVertices(mesh, "body")
for i, vertexIndex in ipairs(indices) do
    print("Vertex:", vertexIndex)
end
```

---

### `verticePos(obj: Object, index: number) → x, y, z`
Gets the local position of a mesh vertex. Use a 1-based render-vertex index, such as one returned by `grupoVertices`.

**Parameters:**
- `obj`: Mesh object
- `index`: Vertex index

**Returns:** Three numbers: x, y, z

**Example:**
```lua
local x, y, z = verticePos(mesh, indices[1])
```

---

### `setVerticePos(obj: Object, index: number, x: number, y: number, z: number)`
Sets a mesh vertex's local position. Use a 1-based render-vertex index.

**Parameters:**
- `obj`: Mesh object
- `index`: Vertex index
- `x, y, z`: New position

**Returns:** None

**Example:**
```lua
setVerticePos(mesh, indices[1], 1, 2, 3)
```

---

### `setVerticeColor(obj: Object, index: number, r: number, g: number, b: number [, a: number])`
Sets the color of a vertex.

**Parameters:**
- `obj`: Mesh object
- `index`: Vertex index
- `r, g, b`: Color (0.0 to 1.0)
- `a`: Optional alpha (defaults to 1.0)

**Returns:** None

**Example:**
```lua
setVerticeColor(mesh, indices[1], 1.0, 0.0, 0.0, 1.0)  -- red
```

---

### `setVertices(obj: Object, vertices: table)`
Sets multiple vertices in one call. `vertices` is a flat sequence of 4-value groups: `{index, x, y, z, ...}`; indices are 1-based.

**Parameters:**
- `obj`: Mesh object
- `vertices`: Flat table of vertex index and local position tuples

**Returns:** None

**Example:**
```lua
local indices = grupoVertices(mesh, "body")
setVertices(mesh, { indices[1], 1, 2, 3, indices[2], 4, 5, 6 })
```

---

## 3D Positioning (Advanced)

### `pos3(obj: Object) → x, y, z`
Gets the object's local position (legacy alias for `Position`).

**Parameters:**
- `obj`: Object

**Returns:** Three position coordinates

**Example:**
```lua
local x, y, z = pos3(obj)
```

---

### `setPos3(obj: Object, x: number, y: number, z: number)`
Sets the object's local position (legacy alias for `SetPosition`).

**Parameters:**
- `obj`: Object
- `x, y, z`: Position

**Returns:** None

**Example:**
```lua
setPos3(obj, 0, 5, 0)
```

---

## Physics

These physics bindings keep their Spanish names in the current Lua API.

### `velocidad(obj) → vx, vy, vz` / `velocidad(obj, vx, vy [, vz])`
Reads an object's velocity, or sets it. Setting velocity makes the physics step move the object each frame. In 2D, coordinates are canvas pixels per second; in 3D, they are engine units per second.

```lua
velocidad(ball, 180, 120) -- set X/Y velocity
local vx, vy, vz = velocidad(ball)
```

### `acelerar(obj, factor [, limit]) → speed`
Multiplies an existing velocity by `factor`, preserving direction. An optional positive `limit` caps the resulting speed.

```lua
local speed = acelerar(ball, 1.05, maxSpeed)
```

### `caja(obj) → width, height, depth` / `caja(obj, width, height [, depth])`
Reads the active collision-box size or assigns a custom size.

```lua
caja(ball, 24, 24) -- 2D collision box in canvas pixels
```

### `rebotar(obj, other [, other2, ...]) → hit`
Checks for overlap with one or more objects (or a table of objects), separates the moving object, and reverses the collision-axis velocity. The first object must have velocity set.

```lua
if rebotar(ball, {leftWall, rightWall}) then beep(440, 60, 0.4) end
```

### `rebotarEn(obj, area [, sides]) → hit`
Keeps an object within an area's collision box. `sides` can name the closed boundaries; omitted sides remain open. It also accepts numeric bounds: `rebotarEn(obj, x0, y0, x1, y1 [, z0, z1])`; pass `nil` for an open boundary.

```lua
rebotarEn(ball, court, "arriba abajo")
```

### `rebotarDentro(obj, area) → hit`
Keeps an object inside all boundaries of the specified area.

```lua
rebotarDentro(ball, court)
```

## Additional registered helpers

### Logs

- `logQuantity() → count`: number of buffered log lines (may be zero in production builds).
- `logLine(index) → message, level`: returns the 1-based log line and its level (`"info"`, `"aviso"`, or `"error"`); out-of-range indices return empty strings.

```lua
for i = 1, logQuantity() do
    local message, level = logLine(i)
    print(level .. ": " .. message)
end
```

### Visibility, collections, and scenes

- `setSector(mesh, sector)`: selects a 1-based sector for a mesh's culling modifier; `0` selects the full mesh.
- `visibilityCell(obj) → cell | nil`: reads an object's current visibility cell, or `nil` if it is not participating.
- `setVisibilityAnchor(zone, obj)`: sets the object used to determine a visibility zone's active cell.
- `setVisibilityCurve(zone, t [, rail])`: updates a curve-driven zone with a progress value and optional rail name/object.
- `setCollection(collection, mode)`: changes a collection's static-batch mode (for example `"static"` or `"off"`).
- `cambiarEscena(name [, restart])`: requests a switch to the named scene; optional `true` requests a restart.

```lua
setVisibilityCurve(tunnelZone, railOf(camera) == "TunnelRail" and 0.5 or 0)
cambiarEscena("NextLevel")
```

These operations depend on the relevant editor/runtime feature hook being available. The request is processed by the game loop; it does not synchronously replace the active scene inside the current callback.

### Vertex batch data

`setVertices(mesh, values)` accepts a flat array of `{index, x, y, z, ...}` groups. It does not accept a vertex-group name or a table of `{x=..., y=..., z=...}` records. Use `grupoVertices(mesh, groupName)` to get 1-based vertex indices, then pass the desired indices and coordinates:

```lua
local body = grupoVertices(mesh, "body")
if #body >= 2 then
    setVertices(mesh, {body[1], 0, 1, 0, body[2], 1, 1, 0})
end
```

This reference lists the Whisk3D globals registered by the runtime. For more complete game examples, see the samples above and the project's example games.
