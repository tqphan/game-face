# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

game-face turns facial expressions into keyboard and mouse input. It's a Qt 6 C++/QML app: MediaPipe Face Landmarker (through its C API) scores blendshapes (0–100) on each camera frame, and user-defined bindings map those scores to input commands, which the separate `input_devices_simulator` library injects into the OS. It replaces an earlier PySide6 + Vue app (removed in commit `d8c3135`; it's in the git history before that).

```powershell
# Qt's MinGW kit is the only Qt kit installed locally
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64
cmake --build build
# running needs Qt's DLLs: add C:\Qt\6.10.1\mingw_64\bin to PATH
ctest --test-dir build -L core --output-on-failure        # all core tests
build\app\core\tests\tst_expression.exe logic             # one suite, one test function
build\tools\face-probe\face-probe.exe --image some-face.jpg
```

Core tests (Qt Test, `app/core/tests`) never inject input or use the camera, so they're safe to run. Their fixtures in `tests/data` are frozen copies of the original app's JSON files.

- `cmake/FetchMediaPipe.cmake` downloads `libmediapipe` (the Tasks C API library) from a pinned GitHub Release of the separate [tqphan/libmediapipe](https://github.com/tqphan/libmediapipe) repo (`MEDIAPIPE_RELEASE`, zip SHA-256 checked) and extracts it with the libraries it needs (Windows: `opencv_world*.dll`) and the MediaPipe/OpenCV licenses; deploy and install copy them all, plus the model `assets/face_landmarker.task`. `third_party/mediapipe_c/mediapipe_c_api.h` is hand-written; re-check it against the MediaPipe tag's `mediapipe/tasks/c` headers whenever `MEDIAPIPE_VERSION` changes.
- `app/core` (no GUI; Qt Core only for JSON and files) loads `libmediapipe` at runtime; `FaceTracker` wraps the face landmarker. The Windows DLL is built with clang-cl against the MSVC runtime but exposes a C ABI, so MinGW builds can use it.
- `BindingEngine` ports the original app's `processSimpleBindings`/`processAdvanceBindings` rule for rule, including that the stop trigger starts "fired". It sends command strings to a callback and never injects anything itself. `Expression` replaces filtrex: a hand-written parser with filtrex's precedence (`not x > 5` is `(not x) > 5`), numbers only, and unknown names rejected at compile time.
- `profiles.cpp` reads the original `user.profiles.json` layout and the older one (`default.profile.json`), and writes the original field names plus `"schemaVersion": 2`.
- PyPI's MediaPipe wheels ≥ 0.10.35 send usage metrics to Google (their libmediapipe is Google's internal build). The tqphan/libmediapipe repo builds libmediapipe in GitHub Actions from the open-source MediaPipe tree instead, which has no such code: Linux x86_64/arm64 (glibc 2.39+), Windows x86_64/arm64 (clang-cl), macOS arm64 (12.0+), with OpenCV 4.14.0 linked in everywhere except Windows x86_64, which ships `opencv_world4140.dll`. CI and release.yml don't build arm64 Linux/Windows yet. Its README covers the build and its checks. Never build MediaPipe locally. The steps for updating MediaPipe are at the top of `cmake/FetchMediaPipe.cmake`.
- `app/ui` is the QML app (`build\app\ui\game-face.exe`). `AppController` is the `App` QML singleton and owns settings, profiles and the camera (`QMediaCaptureSession` feeding the QML `VideoOutput`). `TrackingWorker` runs on its own thread with the tracker, `BindingEngine` and `input_devices_simulator::ScriptRunner`. Only one frame is in flight at a time; frames arriving while the worker is busy are dropped. Commands are injected only while `allow.input.simulation` is on. The list models expose bindings to QML as editable roles, and `LandmarkOverlay` draws the mesh with the scene graph.
- `app/ui/face_connections.h` holds the face mesh connections, extracted once from the MediaPipe tasks-vision JS bundle the original app vendored. The window icon is `assets/icon.png`; `scripts/generate_windows_icon.ps1` makes `packaging/windows/game-face.ico` from it.
- User data lives in `QStandardPaths::AppConfigLocation` (`%LOCALAPPDATA%\game-face` on Windows).
- To check the UI loads without showing a window or touching the camera: set `QT_QPA_PLATFORM=offscreen` and `QT_FORCE_STDERR_LOGGING=1` and run it for a few seconds. A fresh profile has tracking and input simulation off. QML lint: `cmake --build build --target game-face_qmllint`.
- `input_devices_simulator` comes via FetchContent. The sibling checkout `../input_devices_simulator` is used when it exists (`-DIDS_USE_SIBLING=OFF` to skip it); otherwise the commit pinned in `IDS_GIT_TAG` is fetched from GitHub. Bump that pin after pushing library changes the app needs.
- `cmake --install build --prefix "$PWD\dist"` stages a self-contained app (the prefix must be absolute; Qt's deploy script rejects a relative one) (Qt deploy script plus libmediapipe and the model); `release.yml` packages that and signs it with the password-protected "game-face self-signed" certificate. On Windows it signs the exe, then builds `packaging/windows/game-face.iss` (Inno Setup) into a signed installer (see `packaging/signing/README.md`). On macOS, libmediapipe goes in `Contents/Frameworks` and the model in `Contents/Resources`. `ci.yml` builds, runs the core tests and installs on Windows (MSVC), Linux and macOS (arm64) on every push.
- `tools/face-probe` is the phase-2 check: `--image` for a still image, `--camera` for the webcam. Don't run `--camera` yourself; that turns on the user's webcam.

## Binding model

A profile is `{name, bindings[]}`, and `user.profiles.json` is `{selection, items[]}`.
- `simplified: true` → the `simple` block holds one blendshape and a `threshold`. The `pynput.start` command fires when the score rises above the threshold, and `pynput.stop` fires when it drops back below.
- `simplified: false` → the `advance` block holds `start` and `stop`. Each one has an expression (`logic`) evaluated against the blendshape scores, a command, and a `debounce` in ms.
- The command fields are still called "pynput" for historical reasons; the commands are `input_devices_simulator` script (`keyboard.press(...)`, `mouse.move(dx, dy)`, `mouse.position = (x, y)`, …; see its `script.h`).
