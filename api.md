# Whisk3D Lua API reference

This document lists the Lua functions that are currently registered by the runtime.
The source of truth is the C++ registrations in:

- `libs/Whisk3DCore/script/W3dScript.cpp`
- `libs/Whisk3DCore/physics/W3dFisica.cpp`
- `main/W3dEscena.cpp`
- `main/script/BindsJuego.cpp`

The engine calls `start()` and `update(dt)` on attached scripts, but those are lifecycle hooks, not global API functions.
In each signature below, parameters shown without square brackets are required;
parameters inside `[...]` are optional.

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
- Coordinates use the game canvas in pixels, with `(0, 0)` at the center.

### `mouse()`
Returns mouse position and active state.
- Returns: `x, y, active`

### `finger(index)`
Returns the touch data for a finger. Use this with `getUIBox(obj)` when a
control needs to respond to a particular button or to support multiple fingers.
- `index`: finger number; in Lua the engine treats this as 1-based, so `finger(1)` is the first finger.
- Returns: `x, y, active`
- Coordinates use the game canvas in pixels, with `(0, 0)` at the center.

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

### `changeScene(name [, restart])`
Requests a switch to a scene already included in the project. The switch is
applied at the end of the current frame. The destination initializes the first
time it is entered; an initialized scene resumes its existing script state.
Pass `true` for `restart` to reinitialize it.

The legacy name `cambiarEscena(name [, restart])` remains available for
compatibility with existing scripts.

### `changeSceneAsync(name [, activateWhenReady [, callback]])`
Starts preparing a scene asynchronously. Returns `true` when the request is
queued.
- `name`: destination scene name.
- `activateWhenReady`: optional boolean to activate it as soon as it is ready.
- `callback`: optional callback called as `callback(name, progress, status)`;
  progress is between 0 and 1, and status is a string.

### `sceneAsyncReadyLoad(name)`
Returns whether the named scene has completed asynchronous loading.
- `name`: scene name to check.

## 3) Persistent config and script state

### `config("key" [, default])`
Reads a persisted config value.
- `key`: config key string.
- `default`: optional fallback value returned when the key is missing.

### `setConfig("key" [, value])`
Writes a config value.
- `key`: config key string.
- `value`: optional value to store; omitted values are stored as an empty string.

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
Applies a relative uniform scaling factor. With no object argument, the game
layer returns the responsive UI scale factor instead.

### `visible(obj)`
Returns whether the object is visible.

### `setVisible(obj, visible)`
Sets the visibility flag.

### `animator(obj, animationId [, startFrame])`
Requests a vertex-animation clip on a mesh.
- `animationId`: numeric animation identifier.
- `startFrame`: optional frame that switches to the requested animation immediately. Without it, the requested animation is queued after the current clip.

### `rotateTowards(obj, dx, dz [, factor])`
Smoothly rotates an object toward a direction on the X/Z plane.
- `obj`: object to rotate.
- `dx`, `dz`: direction components.
- `factor`: optional interpolation factor; defaults to `0.2`.

### `instantiate(name [, x, y, z])`
Clones a prefab from the project's library and returns the new object, or `nil`
if the prefab is unavailable.
- `name`: prefab name.
- `x`, `y`, `z`: optional initial position in engine coordinates; supply all three.

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

### `setTextura(obj, path)`
Sets the texture path of an image UI object.
- `obj`: image object.
- `path`: project-relative or absolute texture path.

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
Plays a synthesized tone.
- `frequency`: tone frequency in Hz.
- `ms`: duration in milliseconds.
- `volume`: optional volume from 0 to 1.

### `isPressed(obj)`
Returns `true` for one frame when a new touch or click begins inside the object's UI box.

### `isDown(obj)`
Returns `true` while a touch or mouse button is held inside the object's UI box. Use it for
continuous controls such as virtual left/right buttons; use `isPressed` for one-shot actions.

### `isColliding(a, b)`
Returns whether two UI/game regions or objects overlap/collide.

### `clamp(value, min, max)`
Clamps a value into a range.

### `isInside(area [, x [, y]])`
Checks whether a point lies inside a target UI area.
- `area`: target UI object.
- `x`: optional point x-coordinate in canvas pixels; `nil` skips the x-axis test.
- `y`: optional point y-coordinate; `nil` skips the y-axis test.

### `getScale(obj)`
Returns the current visual scale for a UI object.

### `getSafeArea()`
Returns the current safe-area rectangle for the screen.

### `fade(obj, ...)`
Applies a fade/transition on a UI object.

### `getUIBox(obj)`
Returns the current rendered UI box of an object as its center `x, y` and
dimensions `w, h`, in game-canvas pixels.

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

### `setSector(obj, sector)`
Selects the active 1-based visibility/PVS sector for an object. Sector `0`
shows the full mesh.
- `obj`: object with a culling/PVS modifier.
- `sector`: sector index, or `0` for the whole mesh.

### `visibilityCell(obj)`
Returns the active visibility cell for an object, or `nil` if it is not part of
a visibility system.
- `obj`: object participating in visibility culling.

### `setVisibilityAnchor(zone, obj)`
Sets the object whose position selects the active cell in a visibility zone.
- `zone`: visibility zone.
- `obj`: anchor object.

### `setVisibilityCurve(zone, t [, rail])`
Updates a curve-driven visibility zone.
- `zone`: visibility zone.
- `t`: fractional position along the curve.
- `rail`: optional curve object or curve name.

### `setCollection(collection, mode)`
Enables or disables static batching for a collection.
- `collection`: collection object.
- `mode`: `"static"` to enable batching or `"off"` to disable it.

### `emitter(obj [, count])`
Emits a particle burst immediately.
- `obj`: particle emitter object.
- `count`: optional burst count; defaults to `1`.

### `quot()`
Requests that the game exit.

## 9) Physics helpers

These APIs update simple engine-managed velocities and collisions. In builds
without physics support they remain registered as safe no-op stubs.

### `velocity(obj)`
Returns the object's velocity as `vx, vy, vz`.
- `obj`: object with a physics body.

### `velocity(obj, vx, vy [, vz])`
Sets velocity; the physics step moves the object each frame.
- `obj`: object to move.
- `vx`, `vy`: required velocity components.
- `vz`: optional third component; defaults to zero.

### `accelerate(obj, factor [, cap])`
Multiplies the object's current velocity and returns its resulting speed.
- `obj`: object with a physics body.
- `factor`: velocity multiplier.
- `cap`: optional maximum speed; zero or omitted means no cap.

### `collisionBox(obj)`
Returns the active collision box size as `width, height, depth`.
- `obj`: object to query.

### `collisionBox(obj, width, height [, depth])`
Sets the collision box size.
- `obj`: object to configure.
- `width`, `height`: required dimensions.
- `depth`: optional depth; defaults to zero.

### `bounce(obj, other [, ...])`
Separates the first object from overlapping objects and reflects its velocity.
Returns `true` if a collision caused a bounce. Each `other` may be an object or
a table of objects.
- `obj`: moving object with velocity.
- `other`: one or more objects or object tables.

### `bounceWithin(obj, area [, sides])`
Keeps the object inside an area's bounds, bouncing only against the named sides.
Alternatively, pass numeric bounds as
`bounceWithin(obj, x0, y0, x1, y1 [, z0, z1])`; `nil` bounds remain open.
- `obj`: moving object with velocity.
- `area`: area object, or numeric bounds.
- `sides`: optional side names; omitted means all sides.

### `bounceInside(obj, area)`
Keeps the object inside all sides of an area's bounds.
- `obj`: moving object with velocity.
- `area`: boundary object.

## 10) Notes and compatibility

- The runtime intentionally registers English names in the current code (`key`, `keyPressed`, `object`, `property`, `setShared`, `screen`, `isColliding`, `clamp`, `getUIBox`, and so on).
- Legacy Spanish names are still accepted in a compatibility path when the engine calls old script aliases, but the primary API is English-based.
- Script lifecycle hooks remain:
  - `start()`
  - `update(dt)`

The API is still evolving, so this file should be treated as the current verified snapshot rather than a rigid formal specification.
