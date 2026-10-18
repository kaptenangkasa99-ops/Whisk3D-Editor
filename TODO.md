* [x] Always save projects in the v5 JSON-plus-folders layout; keep opening legacy formats.
* [x] Migrate legacy projects to v5 on save.
* [x] Fix repeated save/migration state so an opened legacy project remains usable after saving.
* [x] Automatically rebuild exposed Lua properties when the object/script changes or the IDE saves it.
* [x] Add an automated regression scenario: `main/test/scenarios/v5-save.w3dtest` (`v5save`).
* [x] Cross-check Lua API names and signatures against registrations; document only verified names and behavior.
* [x] Add Console filters for Info, Warn, and Error log levels.
* [x] Add optional consecutive duplicate-log grouping with repetition counts.
* [x] Make Console text easier to select across rows and horizontal overflow, with Ctrl+C and Ctrl+A support.
* [x] Replace unsupported `keyDown()` calls in shipped scripting examples with the registered `keyPressed()` API. Project-local scripts need the same spelling.
* [x] Start the script picker in the current project's folder.
* [x] Rename the Lua physics bindings to English and update examples and regression coverage.
* [x] Fix `chocantest` to call the registered `getUIBox`, `isColliding`, and `clamp` bindings; add a runnable scenario.
* [x] Test save and load features
* [x] Fix "Got force close after save and load" (covered by the v5 save/load regression scenario)
* [x] Add headless mode for automated test cases (implemented via `--script` and scenario-based test runs)
* [x] Create `api.md` listing the exposed Lua functions and parameter descriptions.
* [x] Create a sample game animation showcase demonstrating animator usage.
* [x] Add a UI touchpad/joystick pattern and mapped keyboard-button sample; the virtual pad maps to WASD-style movement based on position.



[] Your Android support builds the editor APK on a development machine. The Compile Game → Android path can also generate a game APK, but it assumes the editor is running in a desktop checkout with a toolchain and source tree available. It is not currently an on-device APK builder.

The key gaps are:

The game builder depends on files that aren’t in the installed app. It looks for the adjacent Whisk3D-Examples Android template and source code from the Whisk3D and examples checkouts. An installed editor APK has neither checkout. See CompilarJuego.cpp and its generated Android build script.

It invokes a desktop build toolchain. The generated script runs bash, ndk-build, and ./gradlew assembleDebug. Gradle also needs a JDK and Android SDK. Those tools and the expected host environment aren’t bundled or configured by the app. Simply setting ANDROID_NDK on the device would not be enough: the usual NDK compiler tools are built to run on desktop hosts, not as Android apps. See CompilarJuego.cpp.

It relies on shell utilities and desktop-style paths. The build setup uses shell commands and expects the projects to have a particular directory layout. An Android app runs in a sandbox and can’t assume that layout, or arbitrary access to files, exists.

Building an APK is more than compiling C++. A standalone game APK needs Android resources and manifest processing, packaging, signing, and a way to hand the APK to Android’s installer. The app currently creates debug-signed APKs in its build flow; it doesn’t provide an on-device release-signing setup or install flow.

The build needs a device-specific resource strategy. The generated script defaults to multiple ABIs. A device-side first version should target the current device’s ABI—typically arm64-v8a—and manage build time, memory, storage, and cancellation.

There are two sensible directions:

Run the game inside Whisk3D on Android: use the native runtime already shipped in the editor and load/play the project there. This avoids building another APK and is the smaller on-device feature.
Produce a separate game APK on the phone: add a complete Android-native build pipeline: make all required source, SDL, template, and SDK/build tooling available in the app’s sandbox; replace the desktop shell/Gradle assumptions with a device-compatible build and packaging process; then export and install the APK through Android’s user-approved package installer flow.
So the main missing piece is not an Android platform setting. It’s an on-device build environment and packaging/install pipeline; the current game builder is wired for a desktop checkout.