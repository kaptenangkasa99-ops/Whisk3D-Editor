# Whisk3D Lua scripting guide

This engine treats Lua like a component system: a `.lua` file can be attached to any object, and that script runs in its own Lua state. In practice, the pattern is very close to Unity's `MonoBehaviour` style:

- A script is attached to an object.
- The script exposes editable values via `properties`.
- `start()` runs once when play starts.
- `update(dt)` runs every frame.
- You access other objects with `object("Name")`.

The runtime is implemented in the Core and registered in the Lua VM in the engine, so the actions below are the ones actually exposed to the game scripts.

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

If a value is not assigned for that instance, the script default from `propiedades` is used.

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

- `object()` gets an object reference by name.
- `option()` reads a dropdown value chosen in the editor.
- `property()` reads a custom numeric/bool/string value.
- `parameter()` is the numeric alias of `property()`.

#### Example

```lua
local ball = object("Ball")
if ball then
    local x, y, z = position(ball)
    print("Ball:", x, y, z)
end
```

---

### 2.2 Transform / object movement

```lua
position(obj)
setPosition(obj, x, y, z)
move(obj, dx, dy, dz)
rotation(obj)
setRotation(obj, x, y, z)
rotate(obj, x, y, z)
scale(obj)
setScale(obj, sx, sy, sz)
scaleBy(obj, sx, sy, sz)
visible(obj)
setVisible(obj, true_or_false)
```

These are the main transform calls.

- `setPosition(...)` is absolute positioning.
- `move(...)` moves relative to the current transform.
- `setRotation(...)` sets absolute rotation in degrees.
- `rotate(...)` rotates relative to current rotation.

Unity equivalents:

- `setPosition(obj, x, y, z)` ~= `transform.position = Vector3(x, y, z)`
- `move(obj, dx, dy, dz)` ~= `transform.Translate(dx, dy, dz)`
- `setRotation(obj, x, y, z)` ~= `transform.eulerAngles = Vector3(x, y, z)`
- `setVisible(obj, true)` ~= `SetActive(true)` or `Renderer.enabled = true`

---

### 2.3 Input

```lua
key("w")
keyDown("w")
buttonDown("a")
stick("left")
stick("right")
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

if keyDown("space") then
    -- jump once
end

local x, y, active = touch()
local x, y = stick("left")
local pressed = button("a")
```

Unity equivalents:

- `key("w")` ~= `Input.GetKey("w")`
- `keyDown("space")` ~= `Input.GetKeyDown(KeyCode.Space)`
- `stick("left")` ~= `Gamepad left stick vector`
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
mute()
isMuted()
info("message")
warning("message")
error("message")
debug("message")
print("hello")
```

Unity equivalents:

- `config(...)` ~= `PlayerPrefs.GetString(...)`
- `setConfig(...)` ~= `PlayerPrefs.SetString(...)`
- `info(...)` ~= `Debug.Log(...)`
- `warning(...)` ~= `Debug.LogWarning(...)`
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
setTexture(obj, "path/image.png")
show(obj, true)
setOpacity(obj, 0.0, 1.0)
setFontSize(obj, w, h)
```

Use these for HUD, buttons, health bars, menus, and screen-space UI.

Unity equivalent:

- `screen()` ~= `Screen.width`, `Screen.height`
- `setText(obj, text)` ~= `Text.text = ...`
- `setTexture(obj, ...)` ~= `Image.sprite = ...`

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

    local x, y, z = position(player)
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

    if keyDown("space") and not jumpActive then
        jumpActive = true
        vy = property("jump")
    end

    if not key("space") and jumpActive then
        local x, y, z = position(player)
        if y <= yBase then
            jumpActive = false
            setPosition(player, x, yBase, z)
        end
    end

    move(player, vx * property("speed") * dt, vy * dt, 0)

    local x, y, z = position(player)
    if y < yBase then
        setPosition(player, x, yBase, z)
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

    local x, y, z = position(coin)
    local player = object("Player")
    if player then
        local px, py, pz = position(player)
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
| `setPosition(obj, x, y, z)` | `transform.position = new Vector3(x, y, z)` |
| `move(obj, dx, dy, dz)` | `transform.Translate(dx, dy, dz)` |
| `rotate(obj, x, y, z)` | `transform.Rotate(x, y, z)` |
| `setVisible(obj, true)` | `gameObject.SetActive(true)` or renderer enabled |
| `shared("score")` | static/global state |
| `setShared("score", v)` | `GameManager.Instance.score = v` |
| `key("w")` | `Input.GetKey("w")` |
| `keyDown("space")` | `Input.GetKeyDown(KeyCode.Space)` |
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
    local x, y, z = position(player)
    if key("d") then
        move(player, 2 * dt, 0, 0)
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
setPosition(obj, x, y, z)
move(obj, dx, dy, dz)
setRotation(obj, x, y, z)
rotate(obj, x, y, z)
setVisible(obj, true)

-- input
key("w")
keyDown("space")
stick("left")
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
warning("warning")
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

## Object & Script Lifecycle

### `object(name: string) → Object | nil`
Gets an object reference by name from the scene.

**Parameters:**
- `name`: Object name to search for

**Returns:** Object reference or nil if not found

**Example:**
```lua
local player = object("Player")
if player then
    print("Found player")
end
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
    move(self, speed * dt, 0, 0)
end
```

---

### `option(name: string) → string`
Reads a dropdown option value from the current script.

**Parameters:**
- `name`: Option key name

**Returns:** Selected option value

**Example:**
```lua
local difficulty = option("difficulty")
if difficulty == "hard" then
    property("enemyCount") = 10
end
```

---

### `parameter(name: string, default: number) → number`
Numeric alias of `property()`. Used for numeric-only properties.

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

### `position(obj: Object) → x, y, z`
Gets the world position of an object.

**Parameters:**
- `obj`: Object to query

**Returns:** Three numbers: x, y, z coordinates

**Example:**
```lua
local x, y, z = position(player)
print("Player at:", x, y, z)
```

---

### `setPosition(obj: Object, x: number, y: number, z: number)`
Sets the absolute world position of an object (teleport).

**Parameters:**
- `obj`: Object to move
- `x, y, z`: Target coordinates

**Returns:** None

**Example:**
```lua
setPosition(player, 0, 5, 0)  -- teleport to (0, 5, 0)
```

---

### `move(obj: Object, dx: number, dy: number, dz: number)`
Moves an object relative to its current position. Respects physics.

**Parameters:**
- `obj`: Object to move
- `dx, dy, dz`: Movement deltas

**Returns:** None

**Example:**
```lua
function update(dt)
    if key("w") then
        move(player, 0, 0, 5 * dt)  -- move forward
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

### `scale(obj: Object) → sx, sy, sz`
Gets the scale of an object.

**Parameters:**
- `obj`: Object to query

**Returns:** Three numbers: scale x, y, z

**Example:**
```lua
local sx, sy, sz = scale(enemy)
```

---

### `setScale(obj: Object, sx: number, sy: number, sz: number)`
Sets the absolute scale of an object.

**Parameters:**
- `obj`: Object to scale
- `sx, sy, sz`: Target scale values

**Returns:** None

**Example:**
```lua
setScale(explosion, 2, 2, 2)  -- double size
```

---

### `scaleBy(obj: Object, sx: number, sy: number, sz: number)`
Scales an object relative to its current scale.

**Parameters:**
- `obj`: Object to scale
- `sx, sy, sz`: Scale multipliers

**Returns:** None

**Example:**
```lua
scaleBy(ui_button, 1.1, 1.1, 1)  -- 10% bigger
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
    move(player, 0, 0, speed * dt)
end
```

---

### `keyDown(name: string) → bool`
Checks if a key was just pressed (only true for one frame).

**Parameters:**
- `name`: Key name

**Returns:** true only on the frame the key is pressed, false otherwise

**Example:**
```lua
if keyDown("space") then
    startJump()
end
```

---

### `buttonDown(name: string) → bool`
Checks if a gamepad button was just pressed.

**Parameters:**
- `name`: Button name ("a", "b", "x", "y", "start", "select", etc.)

**Returns:** true on the frame pressed, false otherwise

**Example:**
```lua
if buttonDown("a") then
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
- `side`: "left" or "right"

**Returns:** Two numbers: x and y in range [-1, 1]

**Example:**
```lua
local lx, ly = stick("left")
local rx, ry = stick("right")
move(player, lx * speed * dt, 0, ly * speed * dt)
```

---

### `touch() → x, y, active`
Gets the touch/mouse position and state (screen coordinates).

**Parameters:** None

**Returns:** Three values: x, y (in screen pixels), active (true if touching/clicking)

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

**Returns:** Three values: x, y (screen pixels), active (true if touching)

**Example:**
```lua
local x, y, active = finger(0)
```

---

### `mouse() → x, y`
Gets the mouse cursor position (screen coordinates).

**Parameters:** None

**Returns:** Two numbers: x, y in screen pixels

**Example:**
```lua
local mx, my = mouse()
```

---

## Audio

### `sound(path: string, volume: number, pitch: number, loop: bool) → handle`
Plays a sound effect.

**Parameters:**
- `path`: Path to sound file (e.g., "sounds/coin.wav")
- `volume`: Volume (0.0 to 1.0)
- `pitch`: Pitch multiplier (1.0 = normal, 2.0 = double speed)
- `loop`: true to loop, false to play once

**Returns:** Handle to stop the sound later

**Example:**
```lua
local sfx = sound("sounds/coin.wav", 0.7, 1.0, false)
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

### `mute()`
Globally mutes all sound.

**Parameters:** None

**Returns:** None

**Example:**
```lua
mute()
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

### `warning(message: string)`
Prints a warning message to the console.

**Parameters:**
- `message`: Message text

**Returns:** None

**Example:**
```lua
warning("Low on ammo!")
```

---

### `error(message: string)`
Prints an error message to the console and halts the script.

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

### `isDebug() → bool`
Checks if the engine is running in debug mode.

**Parameters:** None

**Returns:** true if debug mode, false otherwise

**Example:**
```lua
if isDebug() then
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

### `setTexture(obj: Object, path: string)`
Sets the texture/image of a 2D image object.

**Parameters:**
- `obj`: Image2D or UI element
- `path`: Path to image file

**Returns:** None

**Example:**
```lua
setTexture(healthBar, "images/health_full.png")
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

### `setFontSize(obj: Object, size: number)`
Sets the font size of a 2D text object.

**Parameters:**
- `obj`: Text2D object
- `size`: Font size in pixels

**Returns:** None

**Example:**
```lua
setFontSize(title, 48)
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

### `clamp(obj: Object, area: Object | minMax, axis: string)`
Constrains a 2D object to stay within a rectangular area.

**Parameters:**
- `obj`: Object to constrain
- `area`: Area object or {min, max} table
- `axis`: Optional axis ("x", "y", or nil for both)

**Returns:** None

**Example:**
```lua
clamp(ball, bounds)  -- keep ball inside bounds
clamp(paddle, bounds, "y")  -- only constrain Y axis
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

### `animate(obj: Object, trackName: string, loop: bool)`
Plays a named animation on an object.

**Parameters:**
- `obj`: Object with animation
- `trackName`: Animation name
- `loop`: true to loop, false to play once

**Returns:** None

**Example:**
```lua
animate(player, "run", true)
animate(enemy, "attack", false)
```

---

### `rotateToward(obj: Object, target: Object | x, y, z)`
Rotates an object to face toward a target or direction.

**Parameters:**
- `obj`: Object to rotate
- `target`: Target object, or x, y, z coordinates

**Returns:** None

**Example:**
```lua
rotateToward(enemy, player)
rotateToward(cannon, 10, 5, 0)
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
move(player, rx * speed * dt, 0, fz * speed * dt)
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

### `random() → number`
Returns a random number between 0 and 1.

**Parameters:** None

**Returns:** Random float [0, 1)

**Example:**
```lua
if random() < 0.5 then
    jumpLeft()
else
    jumpRight()
end
```

---

### `instantiate(obj: Object, name: string) → newObj`
Creates a clone of an object.

**Parameters:**
- `obj`: Object to clone
- `name`: Name for the new object

**Returns:** New object reference

**Example:**
```lua
local newBullet = instantiate(bulletTemplate, "bullet_" .. i)
```

---

### `quit()`
Requests the game to close.

**Parameters:** None

**Returns:** None

**Example:**
```lua
if keyDown("escape") then
    quit()
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

### `controller(index: number) → name`
Gets the name of a connected controller.

**Parameters:**
- `index`: Controller index (0-based)

**Returns:** Controller name string

**Example:**
```lua
for i=0, controllers()-1 do
    print("Controller " .. i .. ": " .. controller(i))
end
```

---

## Color & Lighting

### `color(obj: Object) → r, g, b, a`
Gets the color of an object (for lights/materials).

**Parameters:**
- `obj`: Object (light, material, etc.)

**Returns:** Four numbers: r, g, b, a (0.0 to 1.0)

**Example:**
```lua
local r, g, b, a = color(light)
```

---

### `setColor(obj: Object, r: number, g: number, b: number, a: number)`
Sets the color of an object.

**Parameters:**
- `obj`: Object
- `r, g, b, a`: Color components (0.0 to 1.0)

**Returns:** None

**Example:**
```lua
setColor(light, 1.0, 0.5, 0.0, 1.0)  -- orange
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

### `groupVertices(obj: Object) → groupNames`
Gets the list of vertex groups in a mesh.

**Parameters:**
- `obj`: Mesh object

**Returns:** List of group names

**Example:**
```lua
local groups = groupVertices(mesh)
for i, name in ipairs(groups) do
    print("Group:", name)
end
```

---

### `vertexPos(obj: Object, group: string, index: number) → x, y, z`
Gets the position of a specific vertex.

**Parameters:**
- `obj`: Mesh object
- `group`: Vertex group name
- `index`: Vertex index

**Returns:** Three numbers: x, y, z

**Example:**
```lua
local x, y, z = vertexPos(mesh, "body", 0)
```

---

### `setVertexPos(obj: Object, group: string, index: number, x: number, y: number, z: number)`
Sets the position of a vertex.

**Parameters:**
- `obj`: Mesh object
- `group`: Vertex group name
- `index`: Vertex index
- `x, y, z`: New position

**Returns:** None

**Example:**
```lua
setVertexPos(mesh, "body", 0, 1, 2, 3)
```

---

### `setVertexColor(obj: Object, group: string, index: number, r: number, g: number, b: number, a: number)`
Sets the color of a vertex.

**Parameters:**
- `obj`: Mesh object
- `group`: Vertex group name
- `index`: Vertex index
- `r, g, b, a`: Color (0.0 to 1.0)

**Returns:** None

**Example:**
```lua
setVertexColor(mesh, "body", 0, 1.0, 0.0, 0.0, 1.0)  -- red
```

---

### `setVertices(obj: Object, group: string, vertices: table)`
Sets multiple vertices at once (advanced).

**Parameters:**
- `obj`: Mesh object
- `group`: Vertex group name
- `vertices`: Table of vertex data

**Returns:** None

**Example:**
```lua
setVertices(mesh, "body", { {x=1, y=2, z=3}, {x=4, y=5, z=6} })
```

---

## 3D Positioning (Advanced)

### `pos3(obj: Object) → x, y, z, roll, pitch, yaw`
Gets full 3D position and rotation (extended).

**Parameters:**
- `obj`: Object

**Returns:** Six numbers: position and rotation

**Example:**
```lua
local x, y, z, r, p, y = pos3(obj)
```

---

### `setPos3(obj: Object, x: number, y: number, z: number, roll: number, pitch: number, yaw: number)`
Sets full 3D position and rotation atomically.

**Parameters:**
- `obj`: Object
- `x, y, z`: Position
- `roll, pitch, yaw`: Rotation

**Returns:** None

**Example:**
```lua
setPos3(obj, 0, 5, 0, 0, 45, 0)
```

---

This reference covers all publicly exposed Lua functions in Whisk3D. For more examples and patterns, see the sample games in the project's examples folder.
