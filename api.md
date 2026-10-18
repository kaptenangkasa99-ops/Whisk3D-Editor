# Whisk3D Lua API reference

This document lists the Lua functions that are currently registered by the runtime.
The source of truth is the C++ registrations in:

- `libs/Whisk3DCore/script/W3dScript.cpp`
- `main/script/BindsJuego.cpp`

The engine calls `start()` and `update(dt)` on attached scripts, but those are lifecycle hooks, not global API functions.

## 1) Input and basic helpers

### `key(name)`
Returns `true` while the named key is held down.
- `name`: string, for example `"w"`, `"a"`, `"space"`, `"escape"`.

### `keyPressed(name)`
Returns `true` only on the frame when the key transitions from released to pressed.
- `name`: string key name.

### `buttonPressed(name)`
Returns `true` on the frame the gamepad button is pressed.
- `name`: button name such as `"a"`, `"b"`, `"x"` or `"y"`.

### `stick(name)`
Returns the x/y value of a stick.
- `name`: `"izq"` or `"der"` for left or right stick.
- Returns: `x, y`

### `touch()`
Returns the current touch state for the primary touch slot.
- Returns: `x, y, active`

### `mouse()`
Returns mouse position and active state.
- Returns: `x, y, active`

### `finger(index)`
Returns the touch data for a finger.
- `index`: finger number; in Lua the engine treats this as 1-based, so `finger(1)` is the first finger.
- Returns: `x, y, active`

### `button(name)`
Returns the current pressed state of a button.
- `name`: controller button name.

### `random([min], [max])`
Returns a pseudo-random number.
- If only one argument is given, it is treated as the upper bound and the result is in `[0, max)`.
- If two arguments are given, the result is in `[min, max)`.

### `print(...)`
Logs one or more values using the engine log system instead of the normal stdout console.

## 2) Script instance lookup and shared data

### `object("Name")`
Looks up an object reference by a script-assigned name or by the exact scene object name.
- `Name`: object name string.

### `option("name")`
Reads the dropdown/option value for the current instance.
- `name`: option field name.

### `property("name")`
Reads the property value exposed in the script `properties` table.
- `name`: property name.

### `parameter("name" [, default])`
Legacy numeric helper. It is kept for compatibility and returns the configured property value or the fallback default.
- `name`: property name.
- `default`: optional fallback value when the property is unset.

### `shared("key")`
Reads a value from the shared script state.
- `key`: string key to read.

### `setShared("key", value)`
Writes a value into the shared script state.
- `key`: string key.
- `value`: number, boolean or string.

## 3) Persistent config and script state

### `config("key", default)`
Reads a persisted config value.
- `key`: config key string.
- `default`: fallback value returned when the key is missing.

### `setConfig("key", value)`
Writes a config value.
- `key`: config key string.
- `value`: value to store.

### `saveConfig()`
Saves the current config values to disk.

### `loadConfig()`
Loads config values from disk.

### `silence(enabled)`
Sets the global Lua/game audio mute state.
- `enabled`: boolean.

### `isMuted()`
Returns whether audio is globally muted.

## 4) Logging helpers

### `info(...)`, `logInfo(...)`
Writes an info-level log entry.

### `error(...)`
Writes an error-level log entry.

### `debug(...)`
Writes a debug-level log entry.

### `esDebug()`
Returns whether debug logging is enabled.

### `logQuantity(...)`
Logs a quantity/value message.

### `logLine(...)`
Logs a single line of text.

## 5) Object transform and visibility APIs

These functions act on an object reference returned by `object()` or a specific object handle.

### `type(obj)`
Returns the object type name or type identifier.

### `name(obj)`
Returns the object name.

### `Position(obj)`
Returns the world position of the object.
- Returns: `x, y, z`

### `SetPosition(obj, x, y, z)`
Sets the absolute position of the object.

### `mover(obj, dx, dy, dz)`
Moves the object by a relative offset.

### `rotation(obj)`
Returns the rotation in degrees.
- Returns: `x, y, z`

### `setRotation(obj, x, y, z)`
Sets the absolute rotation in degrees.

### `rotate(obj, x, y, z)`
Rotates the object by a relative angle in degrees.

### `scaleObject(obj)`
Returns the object scale.
- Returns: `x, y, z`

### `setScale(obj, sx, sy, sz)`
Sets the absolute scale.

### `scale(obj, factor)`
Applies a relative uniform scaling factor.

### `visible(obj)`
Returns whether the object is visible.

### `setVisible(obj, visible)`
Sets the visibility flag.

### `pos3(obj)`
Returns the object position as a 3-vector-like value.

### `setPos3(obj, x, y, z)`
Sets the object position using 3 values.

## 6) Lights and simple rendering properties

### `color(light)`
Returns the light tint as `r, g, b` with values in the normalized 0..1 range.

### `setColor(light, r, g, b)`
Sets the light color while preserving the current brightness.

### `energy(light)`
Returns the current light brightness.

### `setEnergy(light, value)`
Sets the light brightness.
- `value`: normalized brightness in the 0..1 range.

## 7) Mesh and vertex helpers

These are advanced helpers used for procedural mesh work.

### `grupoVertices(obj, groupName)`
Accesses a named vertex group.

### `verticePos(obj, groupName, index)`
Reads the position of a vertex from a vertex group.

### `setVerticePos(obj, groupName, index, x, y, z)`
Updates the stored position of a vertex.

### `setVerticeColor(obj, groupName, index, r, g, b, a)`
Sets an RGBA color on a vertex.

### `setVertices(obj, groupName, data)`
Sets vertex data for a mesh group.

## 8) Runtime/game helpers

These functions are registered by the 2D game layer in `main/script/BindsJuego.cpp`.

### `screen()`
Returns the current render canvas size or screen dimensions for the game UI.

### `posPx(obj)`
Returns the position of a 2D object in pixels on the current screen/canvas.

### `setPosPx(obj, x, y)`
Sets a 2D object position in canvas pixels.

### `getScreenPx(obj)`
Returns the object size in pixels.

### `setScreenPx(obj, w, h)`
Sets a 2D object size in pixels.

### `setText(obj, text)`
Sets the text contents of a 2D text or button object.
- `text`: string.

### `show(obj)` / `show(obj, visible)`
Shows or hides a 2D object.

### `setOpacity(obj, alpha)`
Sets the opacity/alpha of a UI object.
- `alpha`: normalized float in 0..1.

### `setFontScreenSize(obj, size)`
Changes the screen-size scale used by a text element.

### `sound(path, volume, pitch, loop)`
Plays a sound effect.
- `path`: relative or absolute asset path.
- `volume`: 0..1, optional.
- `pitch`: 1 = normal pitch, optional.
- `loop`: boolean, optional.
- Returns: sound handle id.

### `stopSound(handle, fadeSec)`
Stops a sound by handle.
- `handle`: sound handle returned by `sound()`.
- `fadeSec`: optional fade-out time.

### `beep(frequency, ms, volume)`
Plays a synthesized beep/tone.

### `isPressed(obj)`
Returns whether a UI/game object is currently pressed/active as a control.

### `isColliding(a, b)`
Returns whether two UI/game regions or objects overlap/collide.

### `clamp(value, min, max)`
Clamps a value into a range.

### `isInside(x, y, area)`
Checks whether a point lies inside a target area.

### `getScale(obj)`
Returns the current visual scale for a UI object.

### `getSafeArea()`
Returns the current safe-area rectangle for the screen.

### `fade(obj, ...)`
Applies a fade/transition on a UI object.

### `getUIBox(obj)`
Returns the current rendered UI box of an object.
- Typical return: `x, y, w, h` or equivalent rect values depending on the call site.

### `screenOf(obj)`
Returns the screen-space data for the given UI object.

### `setRail(obj, railName)`
Assigns a rail to a UI or object.

### `setRailNode(obj, nodeIndex)`
Moves the object to a specific rail node.

### `setRailLookAt(obj, target)`
Sets a rail look-at orientation target.

### `railOf(obj)`
Returns the rail associated with an object.

### `lensOf(obj)`
Returns the lens/camera data for an object.

### `setLens(obj, lensData)`
Sets the lens/camera data for an object.

### `cameraXZ(obj)`
Returns the camera X/Z orientation data.

### `target(obj)`
Returns the target object or data used for camera or tracking logic.

### `parameter(name, default)`
Same compatibility helper described earlier, used in the UI/game layer for properties stored in the runtime project.

### `controllers()`
Returns the connected controller list.

### `controller(index)`
Returns the controller object/state for the given index.

## 9) Notes and compatibility

- The runtime intentionally registers English names in the current code (`key`, `keyPressed`, `object`, `property`, `setShared`, `screen`, `isColliding`, `clamp`, `getUIBox`, and so on).
- Legacy Spanish names are still accepted in a compatibility path when the engine calls old script aliases, but the primary API is English-based.
- Script lifecycle hooks remain:
  - `start()`
  - `update(dt)`

The API is still evolving, so this file should be treated as the current verified snapshot rather than a rigid formal specification.
