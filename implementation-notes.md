# Implementation notes

## Milestone 1

- Preserved the original Dev-C++ renderer and Python scripts under `legacy/` without changing their contents.
- Used vcpkg manifest mode for SDL3 and OpenCV. OpenCV is linked now to validate dependency resolution, but no video processing is implemented in this milestone.
- The SDL application stays open until its window is closed. CTest checks both `--version` and a headless `--smoke-test`, so automated runners do not require an interactive display.
- Verification on Windows x64 passed from the Visual Studio 2022 Developer Command Prompt: `cmake --preset default` configured successfully, `cmake --build --preset default` built `drawvideo.exe`, and `ctest --preset default` passed 2/2 tests. The first minimal OpenCV build took about 10 minutes; later builds can reuse vcpkg's binary cache.
- A visible-window check also passed: `drawvideo.exe` created a native window, handled a close-window request and exited with code 0.
- Added a `builtin-baseline` after current vcpkg rejected a manifest without one. The baseline is the commit of the local `D:\tools\vcpkg` checkout, not the separately versioned `vcpkg.exe` bootstrap release.
- A completion audit found that MSVC was not discoverable from a regular PowerShell session. Kept the shared Ninja preset and documented the required Visual Studio Developer shell instead of introducing a Windows-only generator preset.
- Restored all four files under `legacy/` byte-for-byte to their original Git blobs. This deliberately preserves existing legacy bugs and formatting because Milestone 1 requires reference code to remain unchanged.
- Added a deterministic `drawvideo_sdl_smoke` CTest using SDL's dummy video driver. It covers SDL initialization, window creation, window destruction and shutdown without requiring an interactive desktop.
- Disabled OpenCV's default vcpkg features because Milestone 1 links only `core`, `imgproc` and `videoio`. The default feature set pulled unrelated DNN, Protobuf, G-API, codec and UI dependencies into clean setup and did not complete within the verification timeout.
