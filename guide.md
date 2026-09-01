# Whisk3D Lua scripting guide

This engine treats Lua like a component system: a `.lua` file can be attached to any object, and that script runs in its own Lua state. In practice, the pattern is very close to Unity's `MonoBehaviour` style:

- A script is attached to an object.
- The script exposes editable values via `propiedades`.
- `inicio()` runs once when play starts.
- `actualizar(dt)` runs every frame.
- You access other objects with `objeto("Name")`.

The runtime is implemented in the Core and registered in the Lua VM in the engine, so the actions below are the ones actually exposed to the game scripts.

---

## 1) Script lifecycle

Every script can define these globals:

```lua
propiedades = {
    velocidad = 6,
    salto = 10,
    etiqueta = "jugador",
    activo = true,
    dificultad = { "facil", "normal", "dificil" },
}

function inicio()
    -- runs once when the game starts
end

function actualizar(dt)
    -- runs every frame; dt is seconds
end
```

What this means in Unity terms:

- `inicio()` ~= `Start()`
- `actualizar(dt)` ~= `Update(float dt)`
- `propiedades` ~= serialized fields / inspector values

You can read instance values with:

```lua
local speed = propiedad("velocidad")
local mode = opcion("dificultad")
local ref = objeto("pelota")
```

If a value is not assigned for that instance, the script default from `propiedades` is used.

---

## 2) What functions are actually exposed to Lua

These are the actual global functions registered in the engine. They are not theoretical; they are bound at runtime.

### 2.1 Core / object access

```lua
objeto("NombreDelObjeto")
opcion("nombreDeLaOpcion")
propiedad("nombreDelValor")
parametro("nombreDelValor", default)
```

Purpose:

- `objeto()` gets an object reference by name.
- `opcion()` reads a dropdown value chosen in the editor.
- `propiedad()` reads a custom numeric/bool/string value.
- `parametro()` is the numeric alias of `propiedad()`.

#### Example

```lua
local pelota = objeto("Pelota")
if pelota then
    local x, y, z = posicion(pelota)
    print("Pelota:", x, y, z)
end
```

---

### 2.2 Transform / object movement

```lua
posicion(obj)
setPosicion(obj, x, y, z)
mover(obj, dx, dy, dz)
rotacion(obj)
setRotacion(obj, x, y, z)
girar(obj, x, y, z)
escala(obj)
setEscala(obj, sx, sy, sz)
escalar(obj, sx, sy, sz)
visible(obj)
setVisible(obj, true_or_false)
```

These are the main transform calls.

- `setPosicion(...)` is absolute positioning.
- `mover(...)` moves relative to the current transform.
- `setRotacion(...)` sets absolute rotation in degrees.
- `girar(...)` rotates relative to current rotation.

Unity equivalents:

- `setPosicion(obj, x, y, z)` ~= `transform.position = Vector3(x, y, z)`
- `mover(obj, dx, dy, dz)` ~= `transform.Translate(dx, dy, dz)`
- `setRotacion(obj, x, y, z)` ~= `transform.eulerAngles = Vector3(x, y, z)`
- `setVisible(obj, true)` ~= `SetActive(true)` or `Renderer.enabled = true`

---

### 2.3 Input

```lua
tecla("w")
teclaApretada("w")
botonApretado("a")
stick("izq")
stick("der")
toque()
raton()
dedo(1)
boton("a")
```

Common uses:

```lua
if tecla("d") then
    -- move right
end

if teclaApretada("espacio") then
    -- jump once
end

local x, y, activo = toque()
local x, y = stick("izq")
local presionado = boton("a")
```

Unity equivalents:

- `tecla("w")` ~= `Input.GetKey("w")`
- `teclaApretada("espacio")` ~= `Input.GetKeyDown(KeyCode.Space)`
- `stick("izq")` ~= `Gamepad left stick vector`
- `boton("a")` ~= `Input.GetButton("A")`

---

### 2.4 Audio and sound

```lua
sonido("sonidos/bip.wav", volumen, pitch, loop)
pararSonido(handle, fadeSeg)
beep(frecuencia, ms, volumen)
```

Examples:

```lua
sonido("sonidos/coin.wav", 0.7, 1.0, false)
beep(440, 120, 0.4)
```

Unity equivalents:

- `sonido(...)` ~= `AudioSource.PlayOneShot(...)`
- `beep(...)` ~= a lightweight synthesized sfx tone

---

### 2.5 Shared state between scripts

```lua
compartido("score")
setCompartido("score", 42)
```

This is the engine's global cross-script data channel.

Example:

```lua
setCompartido("score", (compartido("score") or 0) + 1)
```

Unity equivalent:

- `setCompartido(...)` ~= static singleton / static variable / shared state manager
- `compartido(...)` ~= global static value accessed from multiple scripts

---

### 2.6 Config and logging

```lua
config("clave", "valorDefecto")
setConfig("clave", "valor")
guardarConfig()
cargarConfig()
silenciar()
estaMudo()
info("mensaje")
aviso("mensaje")
error("mensaje")
depurar("mensaje")
print("hola")
```

Unity equivalents:

- `config(...)` ~= `PlayerPrefs.GetString(...)`
- `setConfig(...)` ~= `PlayerPrefs.SetString(...)`
- `info(...)` ~= `Debug.Log(...)`
- `aviso(...)` ~= `Debug.LogWarning(...)`
- `error(...)` ~= `Debug.LogError(...)`

---

### 2.7 UI / 2D game objects

These are bound by the game layer:

```lua
pantalla()
posPx(obj)
setPosPx(obj, x, y)
tamPx(obj)
setTamPx(obj, w, h)
setTexto(obj, texto)
setTextura(obj, "ruta/imagen.png")
mostrar(obj, true)
setOpacidad(obj, 0.0, 1.0)
setTam(obj, w, h)
```

Use these for HUD, buttons, health bars, menus, and screen-space UI.

Unity equivalent:

- `pantalla()` ~= `Screen.width`, `Screen.height`
- `setTexto(obj, texto)` ~= `Text.text = ...`
- `setTextura(obj, ...)` ~= `Image.sprite = ...`

---

### 2.8 Camera / viewport helpers

```lua
pantallaDe(obj)
setRiel(obj, curva, nodo)
setRielNodo(obj, valor)
setMiradaRiel(obj, true)
rielDe(obj)
lenteDe(obj)
setLente(obj, fov, cerca, lejos)
camaraXZ()
objetivo()
parametro("nombre", default)
```

These are the camera/gameplay helpers for moving along curves, setting lenses, and using camera-relative motion.

Unity equivalents:

- `camaraXZ()` ~= camera-forward/right plane vectors
- `objetivo()` ~= a player target or follow target reference
- `setRiel(...)` ~= path-following camera controller

---

## 3) Minimal game sample

This is a tiny player controller using the actual functions exposed by the engine.

```lua
propiedades = {
    velocidad = 6.0,
    salto = 12.0,
    aceleracion = 14.0,
}

local jugador = nil
local yBase = 0
local saltoActivo = false

function inicio()
    jugador = objeto("Jugador")
    if not jugador then
        error("No se encontro el objeto Jugador")
        return
    end

    local x, y, z = posicion(jugador)
    yBase = y
    setCompartido("puntos", 0)
    info("Juego listo")
end

function actualizar(dt)
    if not jugador then return end

    local vx = 0.0
    local vy = 0.0

    if tecla("a") or tecla("izquierda") then
        vx = vx - 1
    end

    if tecla("d") or tecla("derecha") then
        vx = vx + 1
    end

    if teclaApretada("espacio") and not saltoActivo then
        saltoActivo = true
        vy = propiedad("salto")
    end

    if not tecla("espacio") and saltoActivo then
        local x, y, z = posicion(jugador)
        if y <= yBase then
            saltoActivo = false
            setPosicion(jugador, x, yBase, z)
        end
    end

    mover(jugador, vx * propiedad("velocidad") * dt, vy * dt, 0)

    local x, y, z = posicion(jugador)
    if y < yBase then
        setPosicion(jugador, x, yBase, z)
        saltoActivo = false
    end
end
```

### A more game-like sample: coin collection

```lua
propiedades = {
    nombre = "Moneda",
    valor = 1,
}

local moneda = nil
local activo = true

function inicio()
    moneda = objeto("Moneda")
    if moneda then
        print("Moneda cargada")
    end
end

function actualizar(dt)
    if not moneda or not activo then return end

    local x, y, z = posicion(moneda)
    local player = objeto("Jugador")
    if player then
        local px, py, pz = posicion(player)
        local dist = math.sqrt((px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2)

        if dist < 1.5 then
            setCompartido("puntos", (compartido("puntos") or 0) + propiedad("valor"))
            setVisible(moneda, false)
            activo = false
            sonido("sonidos/coin.wav", 0.7, 1.0, false)
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
| `propiedades = { ... }` | `[SerializeField]` / inspector editable fields |
| `function inicio()` | `Start()` |
| `function actualizar(dt)` | `Update()` |
| `objeto("Nombre")` | `GameObject.Find("Nombre")` |
| `setPosicion(obj, x, y, z)` | `transform.position = new Vector3(x, y, z)` |
| `mover(obj, dx, dy, dz)` | `transform.Translate(dx, dy, dz)` |
| `girar(obj, x, y, z)` | `transform.Rotate(x, y, z)` |
| `setVisible(obj, true)` | `gameObject.SetActive(true)` or renderer enabled |
| `compartido("score")` | static/global state |
| `setCompartido("score", v)` | `GameManager.Instance.score = v` |
| `tecla("w")` | `Input.GetKey("w")` |
| `teclaApretada("espacio")` | `Input.GetKeyDown(KeyCode.Space)` |
| `sonido("file.wav")` | `AudioSource.PlayOneShot(...)` |
| `info(...)` | `Debug.Log(...)` |
| `error(...)` | `Debug.LogError(...)` |

The main difference is that in Whisk3D the script is attached to a scene object and the engine exposes a direct C++-bound API, while Unity uses C# layer APIs around UnityEngine.

---

## 5) How to make a small game in this project

### Step 1: Add a script to an object

Create or assign a `.lua` file to an object in the editor.

### Step 2: Expose data in the inspector

```lua
propiedades = {
    velocidad = 5,
    score = 0,
    nombre = "Enemigo",
    activo = true,
}
```

These values appear as editable instance values in the editor.

### Step 3: Get references to other objects

```lua
local player = objeto("Jugador")
local enemigo = objeto("Enemigo")
```

### Step 4: Move or react each frame

```lua
function actualizar(dt)
    local x, y, z = posicion(player)
    if tecla("d") then
        mover(player, 2 * dt, 0, 0)
    end
end
```

### Step 5: Share state across scripts

```lua
setCompartido("score", (compartido("score") or 0) + 1)
```

This is how multiple objects coordinate without a complex manager object.

### Step 6: Keep logic simple

Use this pattern:

- `inicio()` for setup and references
- `actualizar(dt)` for movement and checks
- `compartido()` for game-wide state
- `objeto("...")` for object lookup
- one script per object or a few scripts working together

---

## 6) Recommended beginner structure

For a simple game, prefer this layout:

- `Jugador.lua` — movement, input, collisions
- `Moneda.lua` — pickup logic
- `Enemigo.lua` — chase / attack logic
- `Hud.lua` — reads `compartido("score")` and updates UI text

This looks very much like Unity's `PlayerController`, `Pickup`, `EnemyAI`, and `HUD` scripts.

---

## 7) Quick cheatsheet

```lua
-- create/read instance values
propiedad("velocidad")
opcion("dificultad")
objeto("Pelota")

-- transform
setPosicion(obj, x, y, z)
mover(obj, dx, dy, dz)
setRotacion(obj, x, y, z)
girar(obj, x, y, z)
setVisible(obj, true)

-- input
tecla("w")
teclaApretada("espacio")
stick("izq")
toque()
raton()

-- audio
sonido("sfx.wav")
beep(440, 120, 0.5)

-- state
setCompartido("score", 10)
compartido("score")

-- config
config("music", "on")
setConfig("music", "off")

-- log
info("hello")
aviso("warning")
error("bad")
```

---

## 8) Final idea

Whisk3D Lua is not a random Lua environment. It is a Unity-like object scripting layer with:

- object references by name
- inspector-like property values
- lifecycle hooks `inicio()` and `actualizar(dt)`
- shared game state via `compartido()`
- direct transforms, audio, UI, and input hooks

If you think in Unity terms, the mental mapping is:

- object script instance = `MonoBehaviour`
- `propiedades` = serialized fields
- `objeto("X")` = `GameObject.Find("X")`
- `compartido()` = static/global manager state
- `inicio()` = `Start()`
- `actualizar(dt)` = `Update()`

That is the shortest path to writing a game in this engine.
