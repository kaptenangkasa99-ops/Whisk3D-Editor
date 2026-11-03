# Audio template

Open `audio.w3d` and press Play. Tap **Play tone** or press **Space** to play a
synthesized tone. The pitch cycles between notes; `audio.lua` uses the
`beep(frequency, milliseconds, volume)` Lua API, so no external audio file is
required. The empty root-level `BuildSetup` UI lets the standalone game builder
target this 3D project; the playable controls remain attached to the scene.
