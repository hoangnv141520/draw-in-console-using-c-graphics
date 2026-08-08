# drawvideo

`drawvideo` will convert video and legacy drawings into line-based vector animation. Milestone 1 provides a reproducible C++ project foundation and a minimal SDL3 window; video processing is intentionally not implemented yet.

## Prerequisites

- CMake 3.21 or newer
- Ninja
- A C++17 compiler
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set to its installation directory

On Windows with MSVC, install the **Desktop development with C++** workload and run the commands below from **Developer PowerShell for VS 2022** or **Developer Command Prompt for VS 2022**. A regular PowerShell session does not add `cl.exe` to `PATH` automatically.

## Build and test

From the repository root:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The first configure installs SDL3 and OpenCV through the `vcpkg.json` manifest.

## Run

```powershell
.\build\default\drawvideo.exe
```

Close the window to exit cleanly. To check only the executable version:

```powershell
.\build\default\drawvideo.exe --version
```

CTest also runs a headless SDL lifecycle smoke test. It initializes SDL, creates a hidden window using SDL's dummy video driver, destroys the window and shuts SDL down.

## Layout

- `apps/drawvideo/`: executable entry point and SDL application layer.
- `include/`, `src/`: reserved for future project-owned library code.
- `tests/`: reserved for automated tests.
- `assets/sample-legacy.txt`: small legacy-format input sample.
- `legacy/`: original WinBGIm and Python sources, excluded from the new build.
