# Whisk3D Engine Architecture

This document describes how the editor, shared engine code, and game runtime
fit together today. It complements the broader
[project guide](./whisk3d-project-guide.md) and the implementation-status
[runtime data-oriented roadmap](./runtime-data-oriented-roadmap.md).

## 1. System overview

```text
Editor application (main/)
    | authoring, import/export, project UI
    v
Project data: scenes, object hierarchy, meshes, materials, scripts, assets
    | save/load (.w3d)                         | game build/export
    v                                           v
Project container and asset IO            Game runtime entry point
                                                |
                                                v
                                      Shared Whisk3DCore systems
                                      scripts -> transforms -> visibility
                                      -> render submission -> graphics API
                                                |
                                                v
                                      Platform window/input/graphics backend
```

The repository is a hybrid editor and engine rather than a fully separated
editor shell and runtime library. `main/` contains the editor and project
workflows; `libs/Whisk3DCore/` contains shared scene, rendering, scripting,
math, asset, audio, and other engine systems. `libs/WhiskUI/` supplies reusable
UI infrastructure. Some editor-oriented state and behavior still exists in
Core, and the game build excludes editor-only portions where required.

## 2. Authoring model and project data

The editor's object hierarchy is the source of truth for authored scenes.
New logical scenes use a distinct `SceneData` root (`tipo: "escena"` in the
project JSON). World objects and independent UI canvas roots are children of
that scene root; the `UI` object now represents only a 2D canvas and its layout,
palette, and rendering settings. The existing editor collection remains the
project-wide container and is not itself a game scene.

Older projects whose top-level `tipo: "ui"` records act as scenes continue to
load as legacy UI-root scenes. They are not silently rewritten into the new
hierarchy when opened or saved. New scenes use the separate scene/UI model,
and loaded legacy projects retain their original structure unless the user
explicitly recreates their content in new scene roots. This preserves existing
`.w3dui` references and the old single-canvas scene workflow.
Objects carry transforms and may reference meshes, materials, scripts, and
other resources. Parent/child relationships determine inherited transforms.
This is an object-graph authoring model; Whisk3D does not replace it with an
entity-component-system for editing.

The main code areas are:

| Responsibility | Location |
|---|---|
| Application startup and editor shell | `main/app/` |
| Scene and collection behavior | `main/W3dEscena.*`, `main/objects/` |
| Project save/load and game export | `main/io/` |
| Model/project import and export | `main/importers/` |
| Object hierarchy, meshes, materials, cameras, lights | `libs/Whisk3DCore/objects/` |
| Asset storage, virtual filesystem, compression, containers | `libs/Whisk3DCore/io/` |
| Lua runtime and bindings | `libs/Whisk3DCore/script/`, `main/script/` |
| Graphics abstraction and backends | `libs/Whisk3DCore/gfx/`, `platform/` |

Project persistence packages scene and resource data in the `.w3d` project
format. Importers translate external formats into the engine's object and
resource model. The game exporter selects the runtime source set and packages
the project assets for a target rather than using the editor UI as the game
runtime.

The editor Outliner can unload legacy UI-root scene content while retaining a
small file-backed placeholder, so other scenes remain editable without keeping
every UI tree in memory. Reload restores the saved scene; unloading offers Save,
Discard, or Cancel. New `SceneData` roots currently remain resident because
their world hierarchy is stored inline in the project JSON; runtime scene
switching still requires its destination scene to be loaded in the project.

## 3. Runtime frame flow

For a game frame, the major responsibilities are ordered so that gameplay
changes are reflected in rendering:

1. The runtime reads platform input and advances attached Lua scripts through
   `start()` and `update(dt)`.
2. Script and animation changes update the authored objects.
3. The 3D runtime builds a packed transform snapshot from the object hierarchy.
   The hierarchy remains authoritative; the snapshot is derived data.
4. Visibility-aware traversal determines which scene content can be drawn.
5. Eligible render items are captured into render packets, sorted, and
   submitted through the graphics abstraction.
6. The platform backend presents the rendered frame.

The packed transform arrays hold local and world transforms and are rebuilt
before the 3D render pass. This ensures changes from scripts, animation, or
hierarchy edits are reflected without making the packed representation the
editable source of truth.

The packet path is not a universal replacement for the existing renderer.
It handles supported runtime passes; wireframe and light-dependent paths,
editor rendering, and mirror rendering retain legacy paths where required.
Runtime systems execute serially. There is no general job scheduler, because
object mutation and graphics-state ownership are not currently safe to
parallelize.

See the
[roadmap](./runtime-data-oriented-roadmap.md) for implementation phases,
validation scope, and target-platform limitations.

## 4. Is data-oriented design fully applicable?

No. Data-oriented design is useful in Whisk3D, but it should not replace every
part of the engine or the editor's object model.

It fits runtime work that processes many similar items predictably. The current
game runtime already applies it selectively: it derives packed transform
snapshots and render packets from the authored object hierarchy. These
contiguous representations can make transform evaluation and render submission
more efficient while leaving the editor's scene data and project format intact.

The object graph remains a better fit for authoring tasks such as parenting,
selection, editing, and representing varied object types. Scripts can also
change objects dynamically, and graphics backends have ordered stateful
operations; forcing these workflows into packed arrays or parallel jobs would
add synchronization and conversion costs and could make existing behavior
harder to preserve. This matters especially on Whisk3D's range of platforms,
where memory and threading capabilities differ.

The practical approach is therefore hybrid: keep the object graph as the
authoritative authoring model, and introduce data-oriented representations in
measured runtime hot paths where profiling and regression tests justify them.
Keep straightforward serial and legacy-render paths where packed data or
parallel scheduling offers no demonstrated benefit. The roadmap records which
runtime paths currently use this approach and its validation limits.

## 5. Core subsystem boundaries

### Scene and geometry

`libs/Whisk3DCore/objects/` defines the common object graph and reusable scene
resources, including meshes, materials, textures, cameras, and lights. The
editor builds on this model with scene-management and object-type behavior in
`main/`.

### Rendering

High-level drawing goes through `w3dEngine` in
`libs/Whisk3DCore/gfx/w3dGraphics.h`, which abstracts graphics state and
submission. Platform-specific implementations provide the underlying
graphics/window integration. Runtime transform snapshots and render packets
are derived representations used by selected game-render paths; they do not
change the editor's authoring model.

### Scripting and input

Lua scripts are attached to project objects. Core script support is in
`libs/Whisk3DCore/script/`; game/UI bindings and editor integration also live
under `main/script/`. The runtime supplies `start()` and `update(dt)` hooks,
object lookup, transform access, input, and shared script state. See
[`api.md`](../api.md) for the current registered API.

### Persistence, assets, and builds

Project serialization, asset packaging, and game export are handled in
`main/io/`, supported by the Core virtual filesystem and asset/container
utilities in `libs/Whisk3DCore/io/`. Importers under `main/importers/` create
engine resources from external files. Platform-specific startup, build
configuration, and graphics integration live under `platform/`.

## 6. Reading the code

For the editor-to-runtime boundary, follow this path:

1. `main/app/main.cpp` — application and command-line startup.
2. `main/W3dEscena.h` — scene organization and activation.
3. `libs/Whisk3DCore/objects/Objects.h` — object hierarchy and transforms.
4. `libs/Whisk3DCore/objects/Mesh.h` — mesh and material-facing data.
5. `libs/Whisk3DCore/script/W3dScript.h` — script lifecycle and data.
6. `libs/Whisk3DCore/gfx/w3dGraphics.h` — graphics abstraction.
7. `main/io/CompilarJuego.cpp` — generated-game source selection and build.
8. `CMakeLists.txt` and `platform/` — project composition and platform setup.

When tracing a feature, first identify whether it belongs to editor authoring,
shared engine/runtime behavior, project persistence, or a platform backend.
That boundary usually identifies the correct layer to change and the
regressions to validate.
