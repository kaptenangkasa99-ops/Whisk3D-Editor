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