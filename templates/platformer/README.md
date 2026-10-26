# Platformer template

Open `platformer.w3d` and press Play. Move with **A/D** or the arrow keys and
jump with **Space** or **W**. Touch buttons provide the same actions on Android.
The touch buttons use Whisk3D's UI hit-testing, so held movement and jump taps
work with simultaneous touches.
`platformer.lua` contains a small gravity and jump loop that you can extend.
Whisk3D is Y-up, so jump height and the ground height use the player's Y
coordinate; Z is the depth axis in this scene.
