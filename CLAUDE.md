# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

game-face turns facial expressions into keyboard and mouse input. A PySide6 (Qt WebEngine) desktop shell hosts a Vue 3 web UI. MediaPipe Face Landmarker runs in the browser and scores blendshapes (0–100) on each video frame. User-defined bindings map those scores to input commands, which Python injects into the OS.

## Running

```bash
pip install -r requirements.txt   # PySide6, evdev
python src/main.py
```

- The current `src/controller.py` uses **evdev/UInput and runs on Linux only**. It needs write access to `/dev/uinput` (root, or a udev rule / `uinput` group), and it creates the virtual devices when `main.py` is imported, so the app won't start on Windows/macOS as-is.
- `src/controller copy.py` is the older **pynput** version (cross-platform, uses `Key.x` / `Button.left` syntax). Keep it for reference; it's not imported.
- There's no build step, bundler, linter, or test suite. Every frontend dependency (Vue, Bootstrap, MediaPipe wasm and model, filtrex) is vendored under `src/assets/` and loaded directly.
- JS `console` output is forwarded to Python stdout by `WebPage.javaScriptConsoleMessage`, so browser errors show up in the terminal.
- `src/test-tobii.py` is a standalone check that the Tobii Stream Engine `.so` loads. It isn't part of the app.

## Architecture

**Python shell (`src/main.py`)**
- Registers a custom `local://` URL scheme. `LocalFolderHandler` serves files from `src/`, and `local://localhost/` resolves to `index.html`. It sets MIME types explicitly for `.wasm`, `.js`/`.mjs` and `.json`, because the ES module imports and MediaPipe wasm loading depend on them. Absolute paths in JS such as `/assets/mediapipe/wasm` resolve against `src/`.
- `WebPage` automatically grants every permission request (camera).
- `Bridge` is exposed to JS over `QWebChannel` as `window.bridge`. It has three slots: `save_profiles(str)` and `save_settings(str)` write `src/assets/json/user.profiles.json` / `user.settings.json`, and `execute_pynput_command(str)` passes the string to `controller.Controller.execute_command`.
- `MainWindow.closeEvent` asks for confirmation (auto-confirms after 10 s), then tears down view → page → profile in a fixed order so QtWebEngine doesn't crash on exit. Keep that ordering if you change shutdown.

**Web UI (`src/index.html` + `src/assets/scripts/app.js`)**
- One Vue app mounted on `<body id="app">`, using the Options API with `setup()` refs. The markup lives in `index.html` as an in-DOM template. There are no SFCs.
- Settings, profiles, translations and version are loaded with JSON **import attributes** (`import x from "...json" with { type: "json" }`). The user files are therefore read once at page load, and saving writes the file back through the bridge.
- Frame loop: `toggleWebcam` → `video.requestVideoFrameCallback(predict)` → `detectForVideo` → update `mp.bs[categoryName]` → `processBindings` for the selected profile → draw landmarks. `predict` must exit when `video.srcObject` is null. Otherwise a stale callback chain keeps running next to a restarted one and fires every binding twice.
- `saveSettings` restarts the webcam so the confidence settings take effect. `lockUiChanged` saves directly on purpose, to skip that restart.

**Binding model** (schema in `src/assets/json/empty.binding.json`; a profile is `{name, bindings[]}`, and `user.profiles.json` is `{selection, items[]}`)
- `simplified: true` → the `simple` block holds one blendshape and a `threshold`. The `pynput.start` command fires when the score rises above the threshold, and `pynput.stop` fires when it drops back below. `activated` is the hysteresis flag.
- `simplified: false` → the `advance` block holds `start` and `stop`. Each one has a **filtrex** expression (`logic`) evaluated against the blendshape map, a command, and a `debounce` in ms. Expressions are compiled into `fn` by `parseLogic` and validated against `blendshapes.samples.json`. `fn` is runtime-only: it's `null` in saved JSON, and `resetProfiles` rebuilds it on load.

**Command language** (strings in the `pynput` fields, parsed by `Controller.execute_command`)
- Function-call syntax restricted to the `allowed_methods` whitelist: `keyboard.press/release/type`, `mouse.move/click/scroll/position/press/release`, plus the assignment `mouse.position = (x, y)`.
- The evdev controller takes raw evdev names: `keyboard.press(KEY_A)`, `mouse.press(BTN_LEFT)`, `keyboard.type('text')`. The fields are still called "pynput" for historical reasons, and profiles written for the pynput controller (`Key.space`, `Button.left`) won't parse.
- `mouse.position` goes through a second virtual absolute-pointer device sized by the `screen_width`/`screen_height` passed to the constructor (default 1920×1080).

## Conventions

- Settings keys are dotted strings (`settings["lock.ui"]`, `settings["webcam.deviceId"]`). When you add a key, add it to `default.settings.json` too, and in `loadSettings` default it for older `user.settings.json` files that lack it (see the `lock.ui` example).
- Every UI string goes through `translations[settings.language].KEY` (`translations.json`, currently `en` only).
- Binding editor inputs must bind `:disabled="locked"` so they respect the Lock UI toggle.
- `user.profiles.json` and `user.settings.json` are committed and hold real user data. Don't reset them by accident.
