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

**Status: Done (Windows editor/Core scope)**

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
- `runtime-packet-test` / `prueba_runtime_render_packets.w3s` — **PASS** on
  Windows: verifies packet extraction, transforms, opaque grouping, stable sort
  keys, transparent far-to-near order, and the single-packet ordered-PVS rule.
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

Windows editor image parity now covers representative multi-material,
transparent/additive, ordered-PVS, and wall-mirror scenes. Mirrors retain their
existing legacy reflection renderer; the wall-mirror test compares the full
viewport while ordinary scene meshes use packet submission. The packet image
check is not forced on `prueba_espejo.w3s`, whose water geometry is emitted by
the mirror subsystem rather than direct mesh traversal. The existing
`prueba_juego3d.w3s` compile step still needs its production runtime-source
dependency.

Windows check (2026-10-26):

- CMake regenerate and Windows Release editor build — **PASS**. The first
  build attempt used stale generated project files and failed to link
  `RuntimeTransforms.cpp`; regenerating the Visual Studio project resolved it.
- `prueba_runtime_transforms.w3s`, `prueba_cull_herencia.w3s`,
  `prueba_lod.w3s`, and `prueba_lod_aabb.w3s` — **PASS**.
- Fixed Windows CRLF trimming for `newmtl`/MTL directives and normalized the
  `usemtl` material token in both OBJ importers. This prevents carriage returns
  from breaking material-name matching on direct OBJ imports.
- `prueba_runtime_render_packets.w3s`,
  `prueba_runtime_transforms.w3s`, `prueba_runtime_data_sync.w3s`,
  `prueba_visorden.w3s`, `prueba_espejo.w3s`, `prueba_espejo_lamina.w3s`,
  `prueba_espejo_corte.w3s`, `prueba_espejo_pared.w3s`,
  `prueba_cull_herencia.w3s`, and `prueba_lod_aabb.w3s` — **PASS**. The v5
  PVS visibility round-trip and ordered-PVS packet image comparison pass.
- `prueba_materiales.w3s` — **PASS** (61 commands), including byte-for-byte
  legacy/packet viewport parity (10 packets), live transform/material/visibility
  mutation parity, and additive image assertion `(255,166,166)`.
- `prueba_espejo_pared.w3s` — **PASS** (24 commands), including packet image
  parity for direct scene meshes and all mirror pixel assertions.
- The Windows Release editor is the available Windows rendering target; the
  exporter has no generated Windows game target. The editor packet toggle
  exercises the same traversal capture and packet flush code and compares full
  viewport images. The generated-game smoke harness's image comparisons were
  not run after these changes. Android source propagation is present through
  shared `kFuentesBase`, `Mesh.cpp`, and shared OBJ importers; no Android build
  was run as requested.
- `git diff --check` — **PASS**.

Phase 3 is **Done** for the Windows editor/Core implementation: the Windows
Release build, packet extraction/order tests, representative full-viewport
parity checks, PVS/culling checks, materials, and mirror regressions all pass.
Generated Linux/Web runtime image checks after these exact edits remain outside
this Windows-first pass; Android was not built by instruction. Mirrors remain
on their legacy renderer by design, and unchanged output is covered by the
Windows full-viewport mirror regression.

## Phase 4 — Runtime data synchronization and change propagation

**Status: Done (Windows editor/Core scope)**

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

Current synchronization policy and checks:

- The object tree remains authoritative. The game rebuilds the packed transform
  snapshot before each 3D render, so edits made by scripts/animation before the
  frame, hierarchy changes, and newly added objects are reflected without
  maintaining incremental object handles. This is the correctness fallback;
  incremental updates are not implemented.
- `runtime-sync-test` / `tools/pruebas/prueba_runtime_data_sync.w3s` — **PASS**
  on Windows: edits a transform, reparents an object, deletes it, creates a
  replacement, checks rebuilt counts, parent indices, world matrices, and
  compares each rebuild against the recursive transform reference over
  repeated frame mutations. It also verifies visibility exclusion and packet
  reference refresh.
- `runtime-live-sync-image-test PisoA` in `prueba_materiales.w3s` — **PASS**:
  compares the full viewport between legacy and packet rendering after
  transform, material-pointer, and visibility changes, while checking captured
  packet data at each frame.
- Windows Release editor build plus transform, packet, material, PVS/culling,
  LOD, and mirror regression scenarios — **PASS**. Synchronization remains a
  full snapshot rebuild each frame, so there is no incremental result to
  compare; the recursive transform path is the reference oracle. A generated
  Windows game target is unavailable, so these checks execute the shared runtime
  capture/sort/render code through the Windows editor harness.

## Phase 5 — Data-oriented runtime systems and scheduling

**Status: Done (serial runtime systems; Windows measurements)**

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

Current system ordering is serial: script/animation changes -> packed transform
rebuild -> visibility-aware hierarchy traversal -> packet extraction -> packet
sort -> draw submission. Lights, wireframe, and editor rendering retain their
legacy paths. No scheduler or parallel execution has been added because shared
object mutation and fixed-function graphics state are not job-safe as currently
owned.

Windows `runtime-systems-bench` uses 5 warm-up frames followed by 240 measured
frames per render mode, with `glFinish` each frame. It also compares recursive
world-matrix queries to the packed full rebuild and asserts matrix parity.
Results from the Windows Release editor (2026-10-26):

- `prueba_materiales.w3s` (14 scene objects, 10 packets): recursive transform
  queries 0.002 ms/frame, packed rebuild 0.001 ms/frame (parity); retained
  transform arrays 2,660 bytes. Legacy render 0.302 ms/frame, packet render
  0.261 ms/frame; retained packet array 1,560 bytes.
- `prueba_visorden.w3s` (3 scene objects, 1 ordered-PVS packet): transform
  timings rounded below 0.001 ms/frame, parity; retained transform arrays 420
  bytes. Legacy render 0.305 ms/frame, packet render 0.260 ms/frame; packet
  array 120 bytes.

These are small editor scenes, so the sub-millisecond frame numbers are local
measurements, not broad device performance claims. Both runs show a modest
packet-path improvement with small retained arrays. The transform snapshot is
also faster than the recursive reference in the materials case. Runtime system
ordering remains serial because object/script mutation and graphics state are
not currently safe to schedule in parallel. No scheduler was introduced, so a
serial-versus-scheduled comparison does not apply. The existing transform,
packet, image-parity, culling/LOD, material, and mirror regressions all pass.

## Phase 6 — Platform and integration verification

**Status: Done (available Windows target; other targets explicitly excluded)**

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

Integration record (2026-10-26):

- Windows Release editor build — **PASS**. Windows CLI packet, transform,
  synchronization, image parity, mirror, culling/LOD, editor-state, template,
  and save/load checks — **PASS**. The v5 visibility/PVS project round-trip and
  material save/reopen assertions pass.
- The generated-game build was attempted from Windows using
  `prueba_runtime_template_build.w3s`; the exporter invokes Unix shell commands
  (`rm`, `cp`, `true`) through `cmd.exe`, so generation failed before compiling
  game sources. The exporter does not provide a Windows generated-game target.
- Android remains explicitly excluded by user instruction. Shared runtime and
  importer sources propagate through `kFuentesBase` and the Android GLES2
  source list, but no Android build or device run was performed. Follow-up:
  build the shared runtime for Android ABIs and run its packet/sync smoke test
  when that exclusion is lifted.
- Linux and Web generated-runtime builds were not validated here: this Windows
  environment has no Linux compiler/WSL distribution or Emscripten toolchain.
  Follow-up: run `tools/pruebas/prueba_runtime_template.sh` on Linux and build
  the Web target with Emscripten. macOS was not tested because this workspace is
  not on macOS; follow-up is a native macOS editor/runtime build if that target
  is required for release.

Phase 6 is **Done for the available Windows target**, with Linux, Web, Android,
macOS, and generated-Windows-game gaps explicitly recorded above with reasons
and follow-ups. These excluded targets are untested, not passing.
