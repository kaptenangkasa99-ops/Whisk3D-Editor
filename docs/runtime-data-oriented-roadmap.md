# Runtime Data-Oriented Architecture Roadmap

## Goal and boundaries

Move the game runtime toward a data-oriented architecture while preserving the
existing editor workflow and authoring model. The editor's object graph remains
the source of truth for authoring. Runtime data is derived from it, and changes
made by scripts or animation must become visible in the running game without
requiring a different editing API.

This roadmap tracks the runtime architecture work only. It does not claim that
Whisk3D already has a complete data-oriented entity framework or job system implementation.

## Completion rule

A phase is **Done** only when its new feature tests and existing-behavior
regression tests have been run successfully against the implementation, and
the relevant builds have passed. A phase must not be stamped successful if it
breaks an existing editor workflow, project format, runtime behavior, or
supported-platform build. Adding a test without running it does not complete a
phase.

For every phase:

- Record the existing behavior and regression scenarios that must remain
  unchanged before implementation begins.
- Run the relevant existing tests before and after the change. Add or update
  tests for uncovered behavior affected by the change.
- Verify both the new feature and the existing behavior it touches; a passing
  feature-specific test alone is not sufficient.
- If a required test or build fails, fix the regression and rerun it. Keep the
  phase **Pending** (or mark it **Blocked** with the reason) until all required
  checks pass. Do not waive a failing check by changing its expected result
  unless the behavior change is intentional, documented, and approved.
- Record the exact scenarios and build targets run, along with their results,
  under that phase before changing its status to **Done**.

Where a phase intentionally changes behavior, document the user-visible
difference and provide tests for both the intended new behavior and unaffected
existing behavior. Keep the editor authoring workflow and project
compatibility as invariants unless a phase explicitly states otherwise.

## Phase 1 — Packed runtime transforms

**Status: Done**

The compiled game runtime builds a flat transform snapshot from the existing
scene hierarchy before the 3D render pass. The snapshot stores parent indices,
local transforms, and world transforms contiguously, then evaluates world
transforms in parent order. Existing transform queries use the snapshot while
the runtime render scope is active. The snapshot is rebuilt for each render so
script and animation changes are reflected on the next rendered frame. The
editor continues to use its existing object behavior.

Completed work:

- Added packed runtime transform storage and hierarchy flattening.
- Routed runtime transform queries through the active snapshot, retaining the
  existing fallback outside that scope.
- Added per-render rebuild and runtime-only activation.
- Registered the transform implementation in generated game and Symbian source
  lists.
- Documented the runtime snapshot behavior in the Core README.

Regression and validation:

- `tools/pruebas/prueba_runtime_transforms.w3s` — **PASS**. Covers hierarchy
  parent indices, packed local/world matrices, inactive-snapshot fallback,
  snapshot consistency before rebuild, updated results after rebuild, and
  activation/deactivation scope.
- Existing behavior regressions `prueba_cull_herencia.w3s`, `prueba_lod.w3s`,
  and `prueba_lod_aabb.w3s` — **PASS** (3 scenarios).
- Linux CMake configure/build — **PASS**.
- `git diff --check` — **PASS**.

The test coverage verifies the packed transform behavior in the test harness.
It does not replace a separately generated game build or on-device validation
for every target platform. Those checks remain required when the corresponding
platform build or runtime path is changed; this phase's Done status records
only the tests and build listed above.

## Phase 2 — Separate editor-only state from Core runtime data

**Status: Done**

The runtime build now excludes editor-only selection, outliner, edit-mode,
constraint-selection, and object-overlay state and code behind
`W3D_GAME_RUNTIME`. The editor keeps its existing object graph and authoring
behavior. Project loading still applies runtime-relevant visibility,
constraints, transforms, animation, and scene data; editor session selection
and layout state are not required by the game build.

Validation:

- Linux editor CMake build — **PASS**.
- Generated 3D game source-set compile check — **PASS**: all 70 translation
  units from `kFuentesBase` / `kFuentes3D`, plus the OpenGL graphics backend,
  passed `-fsyntax-only` with `W3D_GAME_RUNTIME`, `W3D_SIN_EDITOR`, and the
  generated-game include paths.
- `prueba_fase2_estado_editor.w3s` — **PASS**: object selection, entering and
  leaving Edit Mode, face selection, mesh edit/undo, and wireframe rendering.
- Existing editor/save-load/render regressions — **PASS**:
  `prueba_editor.w3s`, `prueba_lod.w3s`, `prueba_lod_aabb.w3s`,
  `prueba_cull_herencia.w3s`, and `prueba_runtime_transforms.w3s`.
- Shared-template regressions — **PASS**:
  `prueba_plantillas_compartidas.w3s` and `prueba_platformer_touch.w3s`.
- `prueba_template_empaquetado.w3s` — **PASS**: packages the repository-owned
  platformer template, reopens the v5 project with its scene/script/mesh assets,
  runs the editor simulation, and renders the scene. This verifies project
  packaging and editor behavior, not a generated game runtime.
- `tools/pruebas/prueba_runtime_template.sh` — **PASS**: builds the editor,
  exports the shared platformer template, links a generated Linux game against
  the repository-local smoke runtime (not the excluded external
  `game/w3drun.cpp`), then runs it. The runtime loaded the project and Lua
  script, moved the player after injected right-key input, rendered the 3D
  scene and touch controls, and saved a non-empty capture.
- `git diff --check` in the editor repository and Core submodule — **PASS**.

The generated game runtime smoke test uses a local verification loop and
runtime-source override; it does not validate the separately maintained
production `game/w3drun.cpp` or Android/device packaging. Those remain target
integration work, not blockers for this phase's editor/Core separation check.

## Phase 3 — Runtime render-packet extraction and safe sorting

**Status: Pending**

The first runtime packet path is implemented for non-wireframe passes that do
not use scene lights. Visible meshes are collected during the existing
visibility-aware traversal, then submitted from a contiguous array using a
64-bit key. Opaque packets group by pass, material/texture, and geometry before
depth; translucent packets sort back-to-front by camera depth. The editor path
and runtime passes that depend on traversal-time scene-light state retain the
legacy renderer.

Packets are now granular per material group, with each packet retaining the
group's material, index range, and group index. Mixed opaque/translucent groups
can be ordered independently. Meshes using ordered PVS triangle runs retain a
single mesh packet so the existing per-triangle ordering is preserved.

Work:

- Define a packet containing the runtime data required to draw an item without
  depending on editor-only object state.
- Extract packets after transforms and visibility decisions are current.
- Add a 64-bit sort key for compatible render state, including pass,
  material/texture, geometry buffer, and an appropriate depth metric.
- Sort opaque packets to reduce redundant state changes.
- Preserve correct back-to-front ordering and existing blending behavior for
  translucent packets; do not apply opaque sorting rules to transparency.
- Keep supported rendering backends and legacy platforms within their existing
  capability constraints.

Required regression scenarios:

- A render-packet extraction scenario verifying packet count, transforms,
  material/geometry references, and visibility against a known scene.
- An opaque ordering scenario verifying deterministic keys and grouping by
  compatible state.
- A transparency scenario verifying the required draw order and no regression
  in blending.
- A visual or image-comparison scenario for representative editor and runtime
  scenes, plus a build of the affected rendering targets.

Current packet-path validation:

- `tools/pruebas/prueba_runtime_template.sh` — **PASS**: generated Linux game
  builds and runs from the shared platformer template without the external
  runtime loop.
- Runtime extraction scenario — **PASS**: verifies two expected visible mesh
  packets, mesh/material/geometry references, finite camera depth, and captured
  world transforms.
- Runtime visibility scenario — **PASS**: hiding the player removes its packet;
  restoring visibility restores the packet.
- Opaque image parity — **PASS**: the generated platformer frame is
  byte-for-byte identical between the legacy traversal submission and the
  packet-sorted path.
- Transparency scenario — **PASS**: two translucent packets sort far-to-near,
  blending changes the output image, and the runtime returns to the original
  material state after the check.
- Mixed material-group scenario — **PASS**: splits a generated-template mesh
  into opaque and translucent groups, checks packet references and index
  ranges, and asserts stable sorted-key order with opaque work before
  transparency.
- Linux editor build and generated Linux game link/run — **PASS**.
- Android GLES2 native runtime build — **PASS** for `arm64-v8a`,
  `armeabi-v7a`, and `x86_64` using the configured Android NDK.
- The focused culling, mirror, batching, and material regression set was
  rerun after per-group extraction — **9 PASS**: culling inheritance, mirror
  scenarios, static collection batching, materials, and particle batching.
- `prueba_visorden.w3s` visibility and transparent-run assertions — **PASS**;
  its later `zipentrada` assertion is stale: `guardarw3d` now writes the v5
  folder layout, while `zipentrada` only accepts v4 ZIP containers. This is a
  test-format mismatch, not a PVS rendering failure.

Still required before marking this phase Done: compare unchanged output on
representative multi-material scenes and exercise packet sorting with
PVS/Culling and mirrored runtime scenes. The mixed-group smoke scenario
validates packet classification and ordering, but does not yet provide a visual
parity check for that synthetic multi-material scene.
The existing `prueba_juego3d.w3s` compile step still needs its production
runtime-source dependency. The `prueba_visorden.w3s` visibility assertions
pass, but its v4-only archive assertion needs updating for the current v5
folder-based save format. Neither item invalidates the focused Linux smoke
result; both remain verification gaps before claiming the phase complete.

## Phase 4 — Runtime data synchronization and change propagation

**Status: Pending**

Make runtime snapshots and packets respond consistently to live changes while
avoiding unnecessary full rebuilds where practical. The editor graph remains
authoritative; runtime data is a derived representation.

Work:

- Identify which runtime data changes can be tracked safely (transforms,
  hierarchy, mesh/material assignment, visibility, and animation).
- Define invalidation or synchronization rules for each supported mutation.
- Ensure object creation, deletion, reparenting, and resource replacement do not
  leave stale runtime handles or packet references.
- Preserve a correct full-rebuild path as a fallback if incremental updates
  cannot be applied safely.

Required regression scenarios:

- Mutate transforms, hierarchy, visibility, and render resources while the
  game is running and verify the next frame reflects each change.
- Exercise deletion/replacement and confirm no stale references are rendered.
- Compare incremental results against a full rebuild for the same scene state.

Run the existing transform, culling, LOD, and rendering regressions as well as
the mutation scenarios. Mark this phase Done only after all pass, including
repeated changes over multiple frames, with no stale data or regressions in
unchanged behavior.

## Phase 5 — Data-oriented runtime systems and scheduling

**Status: Pending**

Evaluate and introduce explicit runtime systems over packed data where they
provide a measurable benefit. Do not convert the editor authoring model into an
entity-component architecture as part of this phase.

Potential systems include transform propagation, animation updates, visibility
and culling, and render-packet construction. A job system or parallel
execution should be introduced only when supported platforms and data
ownership make it safe, including older/mobile targets.

Work:

- Define system inputs, outputs, and ordering dependencies.
- Move suitable runtime work from pointer-heavy object traversal to packed
  arrays, one system at a time.
- Keep platform-specific threading constraints explicit; retain a serial path
  where needed.
- Measure representative scenes before and after each change.

Required regression scenarios:

- Per-system correctness scenarios covering update order and dependencies.
- A deterministic serial-versus-scheduled result comparison where scheduling
  is introduced.
- A performance benchmark on representative scenes that records frame/update
  cost and memory use; do not mark performance work complete based only on
  functional tests.

Run the existing runtime, editor, and rendering regressions affected by each
system in addition to its new correctness tests. Mark this phase Done only
when all pass and the measured results justify the added architecture without
regressing existing behavior.

## Phase 6 — Platform and integration verification

**Status: Pending**

Validate the completed runtime path across the supported build targets and
document any intentional platform limitations.

Required validation:

- Build the editor and generated-game runtime for each supported target covered
  by the change.
- Run the applicable regression scenarios on each target or its available
  target-specific test environment.
- Verify save/load and editor authoring behavior remain unchanged.
- Record targets that cannot be tested in the available environment rather
  than treating them as passed.

This phase is Done only when all declared target validations pass, existing
behavior regressions pass on the applicable targets, and any explicitly
excluded target has a documented reason and follow-up. An untested target is
not a passing target.
