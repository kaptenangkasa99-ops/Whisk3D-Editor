# Whisk3D en Windows

Se compila con **CMake + Visual Studio (MSVC)**. Nada de Git Bash ni MinGW.

## Requisitos

- **Git** — https://git-scm.com/download/win
- **CMake** — https://cmake.org/download/ (tildá "Add CMake to the PATH").
- **Visual Studio** con el workload **"Desarrollo para el escritorio con C++"** (trae el compilador MSVC y el Windows SDK). Las *Build Tools for Visual Studio* solas también sirven.

> Sirve cualquier Visual Studio moderno — CMake detecta solo el que tengas instalado (acá se usa VS 2026, pero anda igual con 2019/2022).

## Compilar (lo más fácil)

Doble click en `platform\windows\build_windows.bat`, o desde una consola:

```bat
platform\windows\build_windows.bat
```

The script initializes only submodules that are missing. It preserves an
already initialized checkout on its current branch and keeps local changes
intact; it does not force the parent repository's pinned Core gitlink over a
developer's work branch.

Inicializa los submódulos que faltan, configura y compila en **Release**. Los submódulos ya inicializados se conservan en su rama/commit actual, incluidos los cambios locales. El ejecutable queda en `platform\windows\build\Release\whisk3d.exe` (con la carpeta `res\` al lado). Para Debug: `build_windows.bat Debug`.

The build stages the four New Project templates beside the executable, and the
installer includes them. Template lookup is based on the executable/resource
directory, so launching from a Start Menu shortcut or another working directory
still finds them.

## A mano (es lo que hace el script)

Desde la **raíz del repo**:

```bat
REM Inicializar una ruta solo cuando aun no esta clonada:
git submodule update --init --recursive <ruta-del-submodulo>
cmake -S . -B platform\windows\build
cmake --build platform\windows\build --config Release
```

CMake usa el Visual Studio que tengas y compila **x64** por defecto. Para forzar arquitectura, agregá al *configure* `-A x64` o `-A Win32` (Windows 32 bits).

## Instalador (.exe)

Con [NSIS](https://nsis.sourceforge.io/Download) instalado, desde `platform\windows\build`:

```bat
cpack -G NSIS
```

Genera `Whisk3D-<fecha>-win64.exe` (accesos directos + asociación de `.w3d`). Sin NSIS, `cpack -G ZIP` hace un `.zip` portable.

## Correr

```bat
platform\windows\build\Release\whisk3d.exe
```

Busca `res\` **al lado del .exe** (el build ya la copia ahí).

To open a project from a terminal and record extra English checkpoints in
`whisk3d.log` beside the current working directory, run:

```bat
platform\windows\build\Release\whisk3d.exe --verbose --open "C:\path\to\project.w3d"
```

The normal editor log is written whether or not `--verbose` is used. The flag
adds checkpoints around project reset, parsing, layout setup, and property
synchronization.

## Compile a Windows game from the editor

The **Compile game** target `Windows .exe` builds into the project's
`platform-build/windows` staging folder and copies the runnable output to
`build/windows/<project>.exe`. It requires:

- CMake and the Visual Studio C++ workload used to build the editor.
- Git for Windows, including `bash.exe` (set `W3D_BASH_PATH` if Bash is not on
  `PATH` or installed under the standard Git directory).
- For 2D projects, `Whisk3D-Examples/game/w3drun.cpp` in the sibling examples
  checkout. For 3D projects, the editor uses its bundled runtime at
  `main/runtime/w3drun3d.cpp`. A custom runtime can be selected with
  `W3D_RUNTIME_SOURCE`; its companion `w3drun.h` must be in the same directory.

The SDL2 source is built from this repository's `thirdparty/SDL2`; OpenGL and
GLU come from Windows. Build output and compiler diagnostics are retained in
`platform-build/windows/build.log`.

The same build can be started without opening the editor window; progress is
printed to the terminal while the full compiler output is saved to
`platform-build/windows/build.log`:

```bat
platform\windows\build\Release\whisk3d.exe --build-game "C:\path\to\project.w3d" --platform windows
```

`--platform` accepts `windows`, `linux-deb`, `linux-appimage`, `web`, or
`android`. If omitted, the project's saved target is used. Optional
`--assets loose|packed` and `--debug` override the project's saved settings.
The command returns exit code 0 on success and nonzero on invalid options or
build failure.
