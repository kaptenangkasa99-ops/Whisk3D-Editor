# Streaming and Render Cost on Low-End Hardware

Engine design notes for large stages on modest GPUs (PS1/PS2/Dreamcast/mobile GLES 1.1 type). Everything here is engine mechanics; the numbers in parentheses come from a real project that served as a testbed (a platforming level with ~315 objects, 28 stage chunks, and 43 destructible crates), and are there to provide scale, not because the engine knows anything about that game.

## 1. One texture per PATH, not per material

`Whisk3DCore/objects/TexturaCache.cpp` is the sole creation and destruction entry point for a content `Texture`. It indexes by the requested path and maintains a **refcount**.

Without this cache, every path that encounters a `map_Kd` executes `new Texture()` + `LoadTexture`: the deferred OBJ importer, animated sprites, and the "Load Texture" dialog, all separately. A stage split into N chunks that share the same atlas uploads the atlas **N times** to the GPU (measured: 28 chunks with a 512×512 atlas = 28 MB; 43 crates with 2 atlases = 11 MB). It is the most expensive memory expenditure a scene can have, and it goes unnoticed until the GPU is small.

Important corollaries:

- **Refcount, not just a map.** Without tracking references, memory cannot be freed, and freeing is half the work: without this, opening one project after another leaks all textures from the previous one (including GPU memory) for the remaining lifecycle of the process.
- **Failure memory.** Paths that have already failed are remembered; otherwise, a `.mtl` referencing a non-existent texture causes every material requesting it to hit the disk again (with hundreds of objects, that results in hundreds of failed `fopen` calls per import).
- **The "anchor" crutch is no longer needed.** Prior to the cache, a project could dodge the issue by declaring the `map_Kd` in a SINGLE `.mtl` and sharing the *material* by name across the others. With path-based caching, that is unnecessary: declaring the same texture across N `.mtl` files costs only a single GPU object.

The A/B comparison can be measured using the same binary and scene: harness command `texcache off|on` (see `tools/pruebas/prueba_texcache.w3s`) and `texinfo` for the counts.

## 2. One draw call per atlas, and sorting by texture

If each group of objects shares an atlas, sorting by texture comes almost for free, and the worst-case scenario drops to a handful of binds per frame (measured: stage → crates → collectibles → enemies → character = **5 binds**).

What the engine does NOT do yet: the render loop traverses the scene tree in hierarchy order, not in material order. With 5 distinct textures it does not matter; with 50, sorting by material prior to submission is required.

## 3. The unit of streaming is the visible chunk

The engine does not enforce a load policy: the natural unit is the object/zone that `Culling` or the PVS already uses as a sector. What the engine provides for this:

- `Culling` with clipping against the active camera's frustum (`soloCamaraActiva`), so that what is previewed matches what is drawn.
- `LOD` measuring distance **to the world AABB** with hysteresis, measuring from the gameplay camera during runtime (coverage: `tools/pruebas/prueba_lod_aabb.w3s`).
- Per-triangle, per-sector PVS (`formato/pvs-json.md`) to determine visibility without traversing geometry.

## 4. 16-bit Textures

Uploading atlases in 16-bit (RGBA4444 / RGB565) on a GPU without texture compression incurs no noticeable loss when the source art is already palettized, and it halves the peak memory footprint. This is a project-level decision, not an engine one: the engine uploads whatever it is given.