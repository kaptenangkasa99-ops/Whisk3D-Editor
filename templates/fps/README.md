# FPS arena template

Open `fps.w3d` and press **Play**. The sample is a small first-person arena
showing keyboard, gamepad, and touch controls; pursuing enemies; physics-driven
movement and pooled projectiles; and muzzle/bullet particle effects with a
weapon flash light.

## Controls

- **Keyboard:** W/A/S/D or arrow keys to move, Q/E to turn, Space or F to fire.
- **Gamepad:** left stick to move, right stick to aim, A to fire.
- **Touch:** use the on-screen movement, aim, and fire buttons.

The first-person camera follows the hidden player collider and uses the same
horizontal/vertical aim controls as the weapon. Behavior is split across
[`player_controller.lua`](./player_controller.lua),
[`weapon.lua`](./weapon.lua), and [`enemy_ai.lua`](./enemy_ai.lua), attached to
the Camera, GunFlash, and GameScripts objects respectively. They communicate
through `shared()` / `setShared()`:
the player publishes aim/fire/damage state, the weapon publishes hit results,
and enemy AI publishes incoming damage. Bullets are a fixed pool of eight scene
objects; firing reuses an inactive or expired projectile instead of creating
objects at runtime. The sample uses `velocity()`, `collisionBox()`, and
`bounce()` for 3D physics and collision checks.

The scene uses simple included cube geometry. Tune movement, enemy health,
projectile speed, pool size, and particle settings in the individual Lua
scripts and `fps.w3d`.
