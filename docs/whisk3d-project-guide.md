# Whisk3D Editor and Core Project Guide

This document is a practical map of the codebase: what lives where, how the editor and engine are organized, and which subsystems matter most when you work on the project.

## 1. High-level architecture

Whisk3D is split into a few clear layers:

- Editor application: `main/`
  - desktop/editor UI
  - viewport logic
  - import/export tools
  - project file IO
  - undo/history, tool state, menus, dock, settings
- Engine runtime: `libs/Whisk3DCore/`
  - scene graph
  - meshes/materials/textures
  - rendering abstraction
  - math, physics, scripting, audio, filesystem, asset storage
- UI toolkit: `libs/WhiskUI/`
  - reusable cross-platform widgets and drawing primitives
- Third-party dependencies: `thirdparty/`
  - SDL2, Lua, helper libs

The project is not a strict three-tier clean architecture in all places; the code comments are explicit that some editor code still lives in the engine and that parts are being cleaned up over time.

Important design idea:

- `main/` is the editor/editor runtime layer.
- `libs/Whisk3DCore/` aims to be the reusable engine layer.
- `libs/WhiskUI/` is an optional UI library for the editor and other apps.

---

## 2. Repository structure

```text
Whisk3D-Editor/
├── CMakeLists.txt                 # root build definition
├── README.md                      # project overview
├── guide.md                       # user docs
├── beginner-locomotion.md
├── platform/
│   ├── windows/
│   │   ├── build_windows.bat
│   │   └── README.md
│   ├── linux/
│   ├── mac/
│   ├── android/
│   ├── web/
│   └── symbian/
├── main/
│   ├── app/
│   ├── config/
│   ├── edit/
│   ├── importers/
│   ├── io/
│   ├── objects/
│   ├── render/
│   ├── script/
│   ├── test/
│   ├── ui/
│   ├── undo/
│   ├── W3dEscena.*
│   ├── W3dPaletas.h
│   └── ...
├── libs/
│   ├── Whisk3DCore/
│   │   ├── animation/
│   │   ├── audio/
│   │   ├── base/
│   │   ├── gfx/
│   │   ├── io/
│   │   ├── math/
│   │   ├── objects/
│   │   ├── physics/
│   │   ├── script/
│   │   ├── thirdparty/
│   │   ├── video/
│   │   ├── README.md
│   │   └── cmake/
│   ├── WhiskUI/
│   │   ├── core/
│   │   ├── draw/
│   │   ├── text/
│   │   ├── theme/
│   │   ├── widgets/
│   │   └── README.md
│   └── Whisk3D-Core/
│       └── duplicate tree, older name / legacy copy
├── thirdparty/
│   ├── SDL2/
│   ├── lua/
│   └── ...
├── res/
│   ├── config.ini
│   ├── installer/
│   ├── Skins/
│   └── icons / assets
├── tools/
│   ├── build_fuente_bitmap.py
│   ├── genlang.py
│   └── pruebas/
├── docs/
│   └── ...
├── W3D Examples/
│   └── sample .w3d projects
└── CMakeLists.txt
```

---

## 3. Build and build system

The root build is defined by `CMakeLists.txt`.

What it does:

- sets C++17
- generates version header from build date
- discovers source files from `main/` and the engine directories
- adds engine / third-party sources (including music, physics, config, math, audio, graphics)
- links third-party libraries and SDL2/Lua
- sets up platform-specific includes and compile definitions
- copies the `res/` directory next to the built executable

Important notes:

- On Windows, the project uses MSVC and the generated Windows RC file.
- The build script is `platform/windows/build_windows.bat`.
- The script expects `git`, `cmake`, and Visual Studio C++ Build Tools in PATH.
- The build is configured into `platform/windows/build` and outputs the binary in `platform/windows/build/Release`.

The root CMake file is the real source of truth for the project composition. If a missing symbol or missing source is reported, this file often tells you what is intentionally included or omitted.

---

## 4. Core runtime: `libs/Whisk3DCore`

This is the reusable engine layer. It contains a wide range of subsystems:

### 4.1 `base/`
Contains core utilities and platform-neutral support code:

- `crossplatform.h` — compatibility helpers and portable definitions
- `w3dBase.h` — shared base primitives
- `w3dlog.h` / `w3dlog.cpp` — logging API
- `W3dConfig.h` / `W3dConfig.cpp` — persistent key/value config storage
- `W3dInteractionState.h` / `W3dInteractionState.cpp` — editor/runtime interaction state
- `W3dClipboard*` — clipboard wrappers for platform backends

This area is the low-level support layer for the rest of the engine.

### 4.2 `math/`
Math utilities used throughout the engine:

- `Vector3.cpp` / `Matrix4.cpp` / `Quaternion.cpp`
- transforms, rotations, view/projection and math functions

This is central to scene transforms, camera math, animation, and rendering.

### 4.3 `objects/`
The object model is here.

Key files:

- `Objects.h` / `Objects.cpp` — base object graph and scene tree model
- `Mesh.h` / `Mesh.cpp` — mesh data, materials, UVs, vertex groups, armatures, editing metadata
- `Materials.h` / `Materials.cpp` — material representation
- `Textures.h` / `Textures.cpp` — texture handling, GPU uploads, caching
- `CameraBase.h` / `CameraBase.cpp` — camera abstraction
- `Light.h` / `Light.cpp` — light objects
- `RenderColors.h` / `RenderColors.cpp` — render colors used by engine objects
- `VisSet.h` / `VisSet.cpp` — visibility sets / regions
- `W3dConstraint.h` — object constraint metadata

This is the architectural heart of the editor and engine. The main object graph and mesh representation are heavily used by the editor and runtime.

### 4.4 `gfx/`
The renderer abstraction layer.

Key files:

- `w3dGraphics.h` / `w3dGraphics.cpp` — renderer state abstraction
- `w3dTexture.h` / `w3dTexture.cpp` — texture operations and upload helpers
- `w3dPantalla.h` / `w3dPantalla.cpp` — screen/presenting abstraction
- `w3dSafeArea.h` / `w3dSafeArea.cpp` — device-safe area logic
- `w3dParticles.h` — particle system definitions

The comments in `w3dGraphics.h` are especially important: this module intentionally hides OpenGL calls behind a portable API. This is the layer that tries to enable multiple graphics backends.

### 4.5 `io/`
Engine-level file and asset management.

Key files:

- `w3dFilesystem.h` / `w3dFilesystem.cpp` — project/virtual filesystem + mount registration
- `W3dAlmacen.h` / `W3dAlmacen.cpp` — project asset storage and data containers
- `W3dMalla.h` / `W3dMalla.cpp` — custom mesh format support
- `W3dTexto.h` / `W3dTexto.cpp` — text/float parsing for custom formats
- `W3dZip.h` / `W3dZip.cpp` — ZIP packing for project resources
- `w3dCompress.h` / `w3dCompress.cpp` — compression helpers
- `W3dRecursos.h` / `W3dRecursos.cpp` — generic resource refcounting and caching
- `W3dPack.h` / `W3dPack.cpp` — pack/container abstraction for app data

This layer is the backbone for project data persistence and custom file formats.

### 4.6 `animation/`
Animation-related logic.

Key files:

- `Animation.h` / `Animation.cpp` — animation system core
- `VertexAnimation.h` / `VertexAnimation.cpp` — vertex deformation animation
- `SkeletalAnimation.h` / `SkeletalAnimation.cpp` — skeletal animation support
- `Armature2DAnimation.h` / `Armature2DAnimation.cpp` — 2D armature animation for UV/2D workflows

This is heavily tied to mesh deformation and rigging.

### 4.7 `audio/`
Audio subsystem.

Key files:

- `W3dAudio.h` / `W3dAudio.cpp` — sound effects mixer and abstract audio API
- `W3dMusic.h` / `W3dMusic.cpp` — streaming music abstraction
- `W3dVolumen.h` / `W3dVolumen.cpp` — global volume and mute logic
- `W3dAudioSDL.cpp` — SDL-backed audio implementation

These modules are intentionally designed to compile even if a backend is disabled; audio can degrade into a no-op safely.

### 4.8 `physics/`
Minimal physics helpers.

- `W3dFisica.h` / `W3dFisica.cpp`

This is a small physical layer for movement and AABB collision-like behavior, used by scripts and runtime simulation.

### 4.9 `script/`
The Lua scripting layer.

- `W3dScript.h` / `W3dScript.cpp`

This system exposes a Lua runtime for game objects and project scripts.

Important concepts:

- any object can have Lua scripts attached
- scripts can expose object refs, dropdown options, values
- scripts can run `inicio()` and `actualizar(dt)`
- there is a shared state map for cross-script communication
- keyboard, gamepad and touch are bridged to Lua

This is one of the main gameplay/runtime systems in the project.

### 4.10 `video/`
Video playback support.

- `W3dVideo.h` / `W3dVideo.cpp`
- `W3dVideoBackend.h`
- `W3dVideoFFmpeg.cpp`
- `W3dVideoWeb.cpp`

This is platform-dependent video decoding and playback support.

---

## 5. Editor application: `main/`

The editor is not a separate app library; it is the main process and toolset built into the same project. `main/` contains all editor-specific logic, UI, importer, serialization, scene logic, and tooling.

### 5.1 `main/app/`
This is the desktop app bootstrap and runtime shell.

Important files:

- `main.cpp` — main app entry
- `constructor.*` — editor construction / initialization
- `controles.*` — input and control wiring
- `variables.*` — global editor variables and state
- `W3dInitUI.*` — UI initialization
- `W3dDock.*` — platform dock integration

This is where the application start-up, UI bootstrap, and top-level control state are orchestrated.

### 5.2 `main/config/`
Configuration and localization.

- `W3dLang.h` / `W3dLang.cpp` / `W3dLangTabla.h` — language system
- `W3dProfile.h` / `W3dProfile.cpp` — profiling state for frame timing
- `W3dConfigGuardar.cpp` / `W3dProfile.cpp` — persisted editor settings
- `w3dVersion.*` — build/version metadata

This is the project’s user settings and profiling/internationalization layer.

### 5.3 `main/importers/`
Importers for scene and model file formats.

- `import_obj.*` — OBJ import
- `import_fbx.*` — FBX import
- `import_gltf.*` — glTF / GLB import
- `import_w3d.*` — W3D project import
- `import_wobj.*` — WOBJ import
- `export_gltf.*` — glTF export

This is how project models, scenes, and data are fed into the editor/engine.

### 5.4 `main/io/`
Editor project serialization and runtime asset handling.

- `GuardarW3D.*` — save project
- `GuardarVersion.*` — version metadata saved with project
- `lectura-escritura.*` — file open/save, shared file dialogs and import flows
- `W3dContenedor.*` — import assets into project container / package
- `CompilarJuego.*` — compile game/export runtime
- `LuaCompilar.*` — Lua-related build/compilation flow
- `Fuente2D.*` — 2D font support
- `SkinAtlas.*` — UI texture atlas handling
- `Textura2D.*` — texture handling for UI resources
- `Video2DCache.*` — UI video caches

This is the real save/load and packaging subsystem for the editor.

### 5.5 `main/objects/`
Editor-specific object behaviors and object type implementations.

These are not just generic engine objects; many are editor-facing object classes tied into viewport editing and UI:

- `Scene.*` / `Collection.*` / `Camera.*` / `Empty.*` / `Mirror.*`
- `UI.h` and many 2D widget objects (`Boton2D`, `Rect2D`, `Texto2D`, `Imagen2D`, `Slice9`, `Expandir2D`, `Video2D`, etc.)
- `ObjectMode.*` — object editing mode behavior
- `EditMesh.*` — mesh edit mode state
- `Primitivas.*` — primitive generation helpers
- `Particulas.*` — particle emitter objects
- `VisZona.*` — visibility region objects
- `Gamepad.*` — gamepad input object helpers
- `Target.*` — target/constraint relation helpers

This folder is one of the most important parts of the editor and strongly reflects the toolchain rather than pure runtime engine.

### 5.6 `main/render/`
Rendering logic for the editor viewport and UI overlays.

- `render.*` / `EscenaRender.*` — render pipeline for scenes and overlays
- `OpcionesRender.*` — render options, redraw toggles, render mode data
- `UIOverlay.*` — editor viewport overlays

This section is tightly connected to the editor viewport and scene display, especially in relation to visibility, overlays, and the real-time render state.

### 5.7 `main/script/`
The editor-side script runtime and integration layer.

- `SimJuego.*` — game simulation mode
- `BindsJuego.*` — Lua bindings for runtime/game logic
- `ScriptUI.*` — UI integration with scripts
- `W3dScript.cpp` — editor-side script bridge possibly connected with the engine runtime script system

This is where the game/scene scripting API is exposed to the editor and simulation mode.

### 5.8 `main/ui/`
UI system for the editor surfaces.

- `ViewPorts/` — 3D viewport, UI layout, input routing, properties, popup windows
- `GeometriaUI/` — geometry tools and UI helpers
- `W3dColors.*` — editor color palette tools

This is the real UI shell for the editor: panels, properties, viewport layout, hierarchy, toolbars, and dialogs.

### 5.9 `main/undo/`
Undo system.

- `Undo.h` / `Undo.cpp`

This is the editor’s transactional memory for undo/redo operations.

### 5.10 `main/test/`
Testing harnesses and script-based validation tools.

- `W3dScript.*`

This is the project’s smaller validation environment for scripting/test runs.

---

## 6. The core scene and object graph

The central object abstraction is `Object` from `libs/Whisk3DCore/objects/Objects.h`.

It includes:

- parent/child hierarchy (`Parent`, `Childrens`)
- visibility and selection state
- name and identity data
- transform state (`pos`, `scale`, rotation)
- rotation logic with Euler / quaternion / axis-angle synchronization
- script references, scene membership, and editor metadata

This model is the backbone of the editor scene graph. The scene is a hierarchical object tree, not just a list of meshes.

The most important idea is that actual data is stored in a hierarchical object model, and many editor features operate on that tree directly.

---

## 7. Mesh system

The mesh model is defined in `libs/Whisk3DCore/objects/Mesh.h` and implemented in `Mesh.cpp`.

This mesh system supports:

- primitives (cube, plane, UV sphere, etc.)
- vertex positions, normals, UVs, colors
- material groups
- faces and corner topology
- vertex groups
- UV groups
- 2D armatures and UV bone rigging
- animations and deformers
- mesh editing metadata for editor mode

Important distinction in the comments:

- `VertexGroup` is used for 3D skinning and control-point weights
- `UVGroup` is used for UV/2D rigging
- 2D armatures live in the mesh and are separate from 3D armature logic

This is one of the richest subsystems in the engine and one of the hardest parts to understand because the project still mixes editor responsibilities with engine logic.

---

## 8. Rendering system

Rendering is intentionally abstracted through `w3dEngine` in `libs/Whisk3DCore/gfx/w3dGraphics.h`.

This abstraction exists so the rest of the engine does not call raw OpenGL directly. It provides:

- rendering state toggles
- depth and stencil operations
- matrix modes and transformations
- texture binding and sampling state
- material state
- lighting, fog, blending, scissor, viewport operations
- buffer and VBO abstraction

This is a classic engine API layer. It hides platform graphics differences and keeps the rest of the project portable.

Renderer behavior is used by the editor viewport and by the engine runtime as a shared scene render state.

---

## 9. Scene and editor lifecycle

### 9.1 `main/W3dEscena.h`
This file defines the multi-scene system.

It supports:

- multiple scenes inside a project
- scene activation and switching
- lazy scene initialization
- scene binding to Lua runtime and event flow

This is more than a simple object tree: it manages runtime scene changes and activation state so the editor and runtime can behave consistently.

### 9.2 `main/app/main.cpp`
This is the primary app bootstrap for the desktop/editor app. It includes:

- graphics setup
- UI startup
- shared layout input and file browser hooks
- web/desktop differences
- menu callbacks for import and load operations
- resource loading and editor startup logic

This is the top-level control point for the application loop.

---

## 10. Project persistence and `.w3d` data model

The project file format is custom and the engine has explicit support for it through the `io/` and `W3dAlmacen` systems.

Key ideas:

- `.w3d` files are project archives / container formats
- resource data may be embedded or stored in a project container
- some data is compressed and packed using ZIP/compress utilities
- custom mesh file format support exists in `W3dMalla` and related code
- project versioning is tracked in `GuardarVersion` and version generation header

The project is not just a standard 3D file; it is an editor project package. That matters if you’re debugging save/load and import/export behavior.

---

## 11. Import/export pipeline

The importers are in `main/importers/` and are central to the editor workflows.

Supported formats include:

- OBJ
- FBX
- glTF / GLB
- `.w3d`
- WOBJ

The importer path usually ends up building generic engine objects (`Object`, `Mesh`, materials, textures) and then feeding them into the editor scene graph.

This is the main boundary between external content formats and the engine’s internal scene model.

---

## 12. Scripting system

The central scripting contract is in `libs/Whisk3DCore/script/W3dScript.h`.

This system:

- embeds Lua runtime in the engine
- allows object-bound scripts
- exposes object references as properties
- supports dropdowns, numbers, booleans, and strings as script attributes
- supports `inicio()` and `actualizar(dt)` lifecycle functions
- bridges keyboard, touch, mouse, and gamepad input to Lua
- allows script-to-script shared state

This is one of the main gameplay systems and is heavily important for runtime behaviors.

Important caveat:

- the engine runtime and the editor share this concept
- but the editor also has editor-specific script integration in `main/script/`

---

## 13. Audio and volume system

The audio stack is modular and intentionally no-op safe.

- `audio/W3dAudio` handles effects
- `audio/W3dMusic` handles streaming/compressed music
- `audio/W3dVolumen` manages master volume and mute state

This is a good example of the engine trying to separate platform-specific and generic behavior while keeping the app from failing when a backend is disabled.

---

## 14. UI toolkit: `libs/WhiskUI`

`WhiskUI` is a separate UI library intended to be lightweight and cross-platform.

It is organized into:

- `core/` — base UI infrastructure
- `draw/` — drawing primitives and rendering helpers
- `text/` — text rendering and fonts
- `theme/` — theming and colors
- `widgets/` — buttons, panels, form controls, popups, and common components

This is intended as a reusable UI layer that can be used independently of the editor. The README says it was extracted from editor logic and kept as a separate project.

---

## 15. Third-party and platform layer

`thirdparty/` contains the dependencies and platform adapters.

- `SDL2/` — windowing, input, audio, event loop integration
- `lua/` — scripting runtime
- other native/platform helper code as needed

The platform-specific setup is handled under `platform/` with OS-specific directories and helper scripts.

This is where the editor turns into a real desktop, web, Android, or Symbian application.

---

## 16. Important design conventions in the codebase

### 16.1 Cross-platform compatibility is a first-class concern
The code comments repeatedly mention old hardware support and compatibility with mobile/retro platforms. This is not a modern only codebase.

### 16.2 There is a strong mix of engine and editor responsibilities
Many comments note that code still belongs in the editor and should eventually be moved out of the core. For example:

- object editing mode data still sits in engine headers
- render preview and editor-only behavior are not fully separated yet
- some systems are intentionally duplicated or messy during refactors

This means you should expect some subsystem overlap while reading the project.

### 16.3 The object model is central
If you want to understand behavior, start from:

- `Objects.h`
- `Mesh.h`
- `W3dEscena.h`
- `W3dScript.h`
- `CMakeLists.txt`

These files are the best map of how the app behaves at runtime.

### 16.4 The project is file-format and project-data driven
A lot of engine logic revolves around custom formats, project packaging, and runtime asset ownership. This is not simply a “draw triangle” app; it is a full authoring environment with asset packaging and project serialization.

---

## 17. Recommended reading order for new contributors

If you are starting from zero, this is the order that makes the most sense:

1. `CMakeLists.txt` — how the project is assembled
2. `main/app/main.cpp` — app startup flow
3. `libs/Whisk3DCore/objects/Objects.h` — object graph model
4. `libs/Whisk3DCore/objects/Mesh.h` — mesh data model
5. `libs/Whisk3DCore/gfx/w3dGraphics.h` — graphics abstraction
6. `libs/Whisk3DCore/script/W3dScript.h` — scripting layer
7. `main/W3dEscena.h` — scene switching / runtime scene system
8. `main/importers/import_w3d.cpp` — project import flow
9. `main/io/GuardarW3D.cpp` — save path
10. `main/ui/ViewPorts/...` — editor viewport interaction and UI

That path gives you the core mental model without needing to read every file individually.

---

## 18. Practical mental model

The project is best understood as:

- a custom 3D authoring environment for retro-style assets and scenes
- a hybrid engine/editor monorepo
- a serialized project runtime that stores scene graph + assets + scripts + materials + animations
- a platform-portable engine with editor-specific tools layered on top

When debugging the project, always ask:

- is this in the engine core or editor shell?
- is it a scene graph concern or a user-interface concern?
- is it a file format/import concern or runtime rendering concern?
- is this code meant to be cross-platform or editor-only?

Those questions will make the architecture much easier to navigate.

---

## 19. Summary

The project structure is intentionally organized around a few major pillars:

- `main/` = editor and editor runtime infrastructure
- `libs/Whisk3DCore/` = reusable engine and platform-neutral runtime
- `libs/WhiskUI/` = reusable UI toolkit
- `thirdparty/` = dependencies and platform layers

If you are working on a bug, the fastest path is usually:

- find the matching feature in the engine layer
- follow the scene graph / mesh / asset pipeline
- then look at the editor-specific wrapper in `main/`

That sequence consistently matches the code organization and the project’s evolving architecture.
