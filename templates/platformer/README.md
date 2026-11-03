# Platformer template

Open `platformer.w3d` and press Play. Move with **A/D** or the arrow keys and
jump with **Space** or **W**. Touch buttons provide the same actions on Android.
The touch buttons use Whisk3D's UI hit-testing, so held movement and jump taps
work with simultaneous touches.
`platformer.lua` contains a small gravity and jump loop that you can extend.
The ground and player models are included as `ground.obj` and `player.obj`, so
the template does not depend on files from the original authoring machine.
An empty root-level `BuildSetup` UI provides the build target for standalone
game builds; the playable touch controls remain attached to the 3D scene.

For setup, scripting, camera, level, animation, and touch-control guidance,
see the [sample game guide](../../docs/sample-game-guide.md).
Whisk3D is Y-up, so jump height and the ground height use the player's Y
coordinate; Z is the depth axis in this scene.

For a 3D combat sample with gamepad, touch, physics, pooled bullets, and enemy
AI, see the [FPS template](../fps/README.md).
