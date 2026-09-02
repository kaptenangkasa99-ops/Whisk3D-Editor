# Lua Function Rename Tasks

## Summary
The guide.md has been updated with English function names, but the C++ code still registers them with Spanish names. This document lists all functions that need to be renamed in the C++ code.

---

## Core Functions (libs/Whisk3DCore/script/W3dScript.cpp)

### File: `libs/Whisk3DCore/script/W3dScript.cpp`
**Function: `RegistrarAPI()` - starting around line 1045**

All these functions need their `lua_setglobal()` second parameter changed:

| Current Spanish Name | Target English Name | Line | Change |
|---|---|---|---|
| `tecla` | `key` | 1045 | `lua_setglobal(L, "tecla")` → `lua_setglobal(L, "key")` |
| `teclaApretada` | `keyDown` | 1046 | `lua_setglobal(L, "teclaApretada")` → `lua_setglobal(L, "keyDown")` |
| `botonApretado` | `buttonDown` | 1047 | `lua_setglobal(L, "botonApretado")` → `lua_setglobal(L, "buttonDown")` |
| `azar` | `random` | 1048 | `lua_setglobal(L, "azar")` → `lua_setglobal(L, "random")` |
| `objeto` | `object` | 1049 | `lua_setglobal(L, "objeto")` → `lua_setglobal(L, "object")` |
| `opcion` | `option` | 1050 | `lua_setglobal(L, "opcion")` → `lua_setglobal(L, "option")` |
| `propiedad` | `property` | 1051 | `lua_setglobal(L, "propiedad")` → `lua_setglobal(L, "property")` |
| `compartido` | `shared` | 1053 | `lua_setglobal(L, "compartido")` → `lua_setglobal(L, "shared")` |
| `setCompartido` | `setShared` | 1054 | `lua_setglobal(L, "setCompartido")` → `lua_setglobal(L, "setShared")` |
| `toque` | `touch` | 1056 | `lua_setglobal(L, "toque")` → `lua_setglobal(L, "touch")` |
| `dedo` | `finger` | 1057 | `lua_setglobal(L, "dedo")` → `lua_setglobal(L, "finger")` |
| `raton` | `mouse` | 1058 | `lua_setglobal(L, "raton")` → `lua_setglobal(L, "mouse")` |
| `boton` | `button` | 1060 | `lua_setglobal(L, "boton")` → `lua_setglobal(L, "button")` |
| `girarHacia` | `rotateToward` | 1063 | `lua_setglobal(L, "girarHacia")` → `lua_setglobal(L, "rotateToward")` |
| `animar` | `animate` | 1064 | `lua_setglobal(L, "animar")` → `lua_setglobal(L, "animate")` |
| `instanciar` | `instantiate` | 1065 | `lua_setglobal(L, "instanciar")` → `lua_setglobal(L, "instantiate")` |
| `grupoVertices` | `groupVertices` | 1067 | `lua_setglobal(L, "grupoVertices")` → `lua_setglobal(L, "groupVertices")` |
| `verticePos` | `vertexPos` | 1068 | `lua_setglobal(L, "verticePos")` → `lua_setglobal(L, "vertexPos")` |
| `setVerticePos` | `setVertexPos` | 1069 | `lua_setglobal(L, "setVerticePos")` → `lua_setglobal(L, "setVertexPos")` |
| `setVerticeColor` | `setVertexColor` | 1070 | `lua_setglobal(L, "setVerticeColor")` → `lua_setglobal(L, "setVertexColor")` |
| `tipo` | `type` | 1074 | `lua_setglobal(L, "tipo")` → `lua_setglobal(L, "type")` |
| `nombre` | `name` | 1075 | `lua_setglobal(L, "nombre")` → `lua_setglobal(L, "name")` |
| `posicion` | `position` | 1077 | `lua_setglobal(L, "posicion")` → `lua_setglobal(L, "position")` |
| `setPosicion` | `setPosition` | 1078 | `lua_setglobal(L, "setPosicion")` → `lua_setglobal(L, "setPosition")` |
| `mover` | `move` | 1079 | `lua_setglobal(L, "mover")` → `lua_setglobal(L, "move")` |
| `rotacion` | `rotation` | 1081 | `lua_setglobal(L, "rotacion")` → `lua_setglobal(L, "rotation")` |
| `setRotacion` | `setRotation` | 1082 | `lua_setglobal(L, "setRotacion")` → `lua_setglobal(L, "setRotation")` |
| `girar` | `rotate` | 1083 | `lua_setglobal(L, "girar")` → `lua_setglobal(L, "rotate")` |
| `escala` | `scale` | 1086 | `lua_setglobal(L, "escala")` → `lua_setglobal(L, "scale")` |
| `setEscala` | `setScale` | 1087 | `lua_setglobal(L, "setEscala")` → `lua_setglobal(L, "setScale")` |
| `escalar` | `scaleBy` | 1088 | `lua_setglobal(L, "escalar")` → `lua_setglobal(L, "scaleBy")` |
| `energia` | `energy` | 1095 | `lua_setglobal(L, "energia")` → `lua_setglobal(L, "energy")` |
| `setEnergia` | `setEnergy` | 1096 | `lua_setglobal(L, "setEnergia")` → `lua_setglobal(L, "setEnergy")` |
| `guardarConfig` | `saveConfig` | 1099 | `lua_setglobal(L, "guardarConfig")` → `lua_setglobal(L, "saveConfig")` |
| `cargarConfig` | `loadConfig` | 1100 | `lua_setglobal(L, "cargarConfig")` → `lua_setglobal(L, "loadConfig")` |
| `silenciar` | `mute` | 1101 | `lua_setglobal(L, "silenciar")` → `lua_setglobal(L, "mute")` |
| `estaMudo` | `isMuted` | 1102 | `lua_setglobal(L, "estaMudo")` → `lua_setglobal(L, "isMuted")` |
| `aviso` | `warning` | 1104 | `lua_setglobal(L, "aviso")` → `lua_setglobal(L, "warning")` |
| `depurar` | `debug` | 1106 | `lua_setglobal(L, "depurar")` → `lua_setglobal(L, "debug")` |
| `esDebug` | `isDebug` | 1107 | `lua_setglobal(L, "esDebug")` → `lua_setglobal(L, "isDebug")` |

**Total: 40 functions to rename**

---

## 2D Game Functions - ✅ ALREADY CORRECT

### File: `main/script/BindsJuego.cpp`
These are **already registered with English names** in the C++ code and match the guide.md:
- `screen()` 
- `posPx()`
- `setPosPx()`
- `getScreenPx()`
- `setScreenPx()`
- `setTexto()` → `setTexture` (needs renaming in Lua name only)
- `sound()`
- `stopSound()`
- `show()`
- `setOpacity()`
- `setFontScreenSize()`
- `quit()`
- `isPressed()`
- `isColliding()`
- `clamp()`
- `isInside()`
- `getScale()`
- `getSafeArea()`
- `fade()`
- `getUIBox()`
- `screenOf()`
- `setRail()`
- `setRailNode()`
- `setRailLookAt()`
- `railOf()`
- `lensOf()`
- `setLens()`
- `cameraXZ()`
- `target()`
- `parameter()`
- `controllers()`
- `controller()`

---

## Additional Renames Needed in BindsJuego.cpp

The following function names in BindsJuego.cpp should also be updated to match documentation:

| Current Name | Target Name | Purpose |
|---|---|---|
| `setTexto` | `setText` | Set text of 2D text object |

---

## Rename Strategy

1. **Batch edit in W3dScript.cpp**: Find and replace in the `RegistrarAPI()` function around lines 1045-1107
2. **Single edit in BindsJuego.cpp**: If needed, change `setTexto` registration to `setText`
3. **Update function implementations** (C++ function names like `LTecla`, `LObjeto`, etc.) - these don't need to change, only the `lua_setglobal()` second parameter

---

## Testing After Changes

After making these changes, test with:
```lua
-- Old Spanish names should NO LONGER WORK
if tecla("w") then print("Old way") end

-- New English names should WORK
if key("w") then print("New way") end
```

---

## Notes

- The C++ function implementations (like `LTecla`, `LObjeto`) can keep their Spanish names internally
- Only the `lua_setglobal(L, "name")` second parameter needs to change
- The guide.md already documents the English names, so once C++ is updated, everything will be consistent
- The 2D game layer (BindsJuego.cpp) was already using English names, so it's mostly complete
