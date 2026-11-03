# Shared game templates

These starter projects are shared by the editor across platforms:

- `animation/` — animation usage example.
- `audio/` — audio usage example.
- `controls/` — keyboard and virtual-control example.
- `platformer/` — small platformer with keyboard and multi-touch controls.
- `fps/` — first-person arena shooter with keyboard, gamepad, and touch controls,
  enemy pursuit, pooled projectiles, physics collisions, muzzle light, and particle trails.

Keep this directory as the canonical source. Platform packaging may copy these
files into platform-specific application assets, but those copies should be
generated from this directory.

Each starter project has a logical `Scene` root assigned as its initial scene.
Keep referenced model, script, and UI assets inside the template folder and use
relative paths so a newly created project opens without machine-specific paths.
Each project also includes an empty root-level `BuildSetup` UI. It is the
standalone builder's target; playable controls stay attached to the game scene.
