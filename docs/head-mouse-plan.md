# Head mouse: plan

Move the mouse pointer (and scroll) with your head, alongside the existing expression bindings. What drives each axis is up to the user: turning the head, moving it, tilting it, pointing the nose, or any combination.

## Starting point

- MediaPipe already returns a 4×4 head-pose matrix per face (`facial_transformation_matrixes` in [mediapipe_c_api.h](../third_party/mediapipe_c/mediapipe_c_api.h)), but [face_tracker.cpp](../app/core/src/face_tracker.cpp) turns it off (`output_facial_transformation_matrixes = false`), and `FaceFrame` only carries blendshapes and landmarks.
- `input_devices_simulator` has `Mouse::moveBy(int dx, int dy)`, `moveTo(x, y)` and `scroll(dx, dy)` (whole wheel notches) on every platform.
- `Expression` ([expression.h](../app/core/include/game_face/core/expression.h)) already compiles numeric expressions over named variables, with arithmetic, `abs`/`min`/`max`, ternaries, and unknown names rejected at compile time.
- `TrackingWorker::process` ([tracking_worker.cpp](../app/ui/tracking_worker.cpp)) is where it hooks in: it has the face result, the frame timestamp, the simulator and the injection gate.

## Design

The design separates **signals** (numbers measured from the face each frame) from **outputs** (pointer X/Y, scroll X/Y). Each output axis has a **source**: an expression over the signals. Every other setting works the same whatever the source is, because each source is normalised to a common scale first.

### 1. Signals (core)
Each frame produces a fixed, named set of numbers:

| Signal | Meaning | Unit |
|---|---|---|
| `headYaw` | head turned to the user's right is positive | degrees |
| `headPitch` | head tilted up is positive | degrees |
| `headRoll` | head tilted toward the right shoulder is positive | degrees |
| `headX` | head moved to the user's right is positive | cm (approx.) |
| `headY` | head moved up is positive | cm (approx.) |
| `headZ` | head moved toward the camera is positive | cm (approx.) |
| `noseX`, `noseY` | nose tip position in the image, mirrored so right/up is positive | 0–1 of the frame |
| the 52 blendshapes | as in bindings | 0–100 |

- The angles and `headX/Y/Z` come from the transformation matrix. Turn on `output_facial_transformation_matrixes` and add `std::optional<HeadPose> pose` to `FaceFrame`.
- Confirm the matrix's row-major or column-major layout against MediaPipe v1.0.0's `mediapipe/tasks/c/components/containers/matrix.h`, and write the answer down in `mediapipe_c_api.h`.
- The translation is in the canonical face model's units (roughly cm) and depends on the camera's assumed field of view, so it's only meaningful relative to neutral. That's all the mapper needs.
- All signals are defined from the user's point of view (right, up, toward the camera), so the camera's mirroring is handled once, here, and never appears as a per-axis setting.
- `noseX/noseY` is the "image position" style used by tools like Camera Mouse. It mixes rotation and position, which some users prefer. It uses the landmarks that are already there.
- A `Signals` struct holds the values and the shared list of names. The names don't clash with the blendshape names.

### 2. Outputs and sources
There are two output groups, each with two axes:
- **Pointer:** X (right is positive) and Y (up is positive; the worker flips it for screen coordinates).
- **Scroll:** X and Y, in wheel notches, matching `mouse.scroll`.

Each axis has:
- `source`: an expression compiled with `Expression` against the signal names. An empty source means the axis is off. Examples:
  - `headYaw`: turning (the default for pointer X)
  - `headX`: sideways head position instead of turning
  - `headRoll`: tilting instead of turning
  - `headYaw + 2 * headX`: both at once
  - `eyeLookOutRight - eyeLookInRight`: eye direction from blendshapes
- `neutral`: the source's value when centered. Recenter captures it for every axis.
- `range.negative`, `range.positive`: how far the source moves from neutral, in each direction, for a "full" deflection. They can differ, because people usually look down further than up. Each preset fills in sensible defaults (for example 20° for yaw, 4 cm for `headX`), and **Calibrate** measures them (see 6).
- `gain`, `invert`.

A source that fails to compile (for example from a hand-edited settings file) turns that axis off and shows the compile error in the settings panel. It never stops tracking.

Each group has:
- `mode`: **relative** or **joystick** (and **absolute** later, pointer only). Pointer defaults to relative. Scroll defaults to joystick, because holding a tilt to keep scrolling feels natural.
- `deadzone`, `smoothing`.

Defaults: pointer X = `headYaw`, pointer Y = `headPitch`, scroll off.

### 3. `HeadMouse` mapper in core (no Qt, testable)
Each frame it takes `(signals, t_ms)` and returns integer pointer `(dx, dy)` and scroll `(sx, sy)`. For each group:
1. **Evaluate and normalise.** For each axis: `n = (source − neutral) / range` (using the negative or positive range depending on the sign), so ±1 is a full deflection whatever the source and its units.
2. **Smoothing.** Run each normalised value through a One Euro filter. Filtering after normalising means one smoothing setting behaves the same for degrees, centimetres and blendshape scores.
3. **Radial deadzone** on the group's 2D normalised vector, then a response curve, by mode:
   - **Relative.** The head drags a point along behind it (backlash): the point only moves once the head is more than the deadzone away from it. Movement = the point's movement × gain, with acceleration. Tracking noise wobbles around a still head and never gets that far; a real movement loses only the deadzone's width when it starts or turns back. (A deadzone on speed was tried first; the bench showed noise and slow movement have overlapping speeds, so it let 25 px/s of drift through.) It feels like a trackpad, and it works in games that read raw input.
   - **Joystick.** Velocity = normalised offset × gain. Hold a deflection and it keeps moving. Good for FPS camera control, large screens and scrolling.
   - **Absolute (later).** Normalised position maps to a screen position through `moveTo`. Desktop only, because games ignore absolute moves.
4. **Sub-pixel / sub-notch accumulator** per axis, carrying fractional remainders to the next frame. Without it, slow movement stalls. Scrolling especially needs it, since a notch is large.
5. **Frame time (dt).** Only one frame is in flight at a time, so frames get dropped, and the speed must stay the same when they do.
   - Joystick: movement = velocity × dt.
   - Relative: the change already covers the time between frames, so it is **not** multiplied by dt. dt is only used to work out the velocity that drives acceleration (change ÷ dt).
6. **Invert** per axis.

**Neutral when the face comes back:** the saved neutral is kept. If no neutral has ever been saved for an axis, the first frame with a face captures it.

### 4. Settings: global
Stored in `user.settings.json` with the existing flat dotted keys ([settings.cpp](../app/core/src/settings.cpp)) and read leniently (any missing key uses its default):
- `head.mouse.enabled`
- per group (`pointer`, `scroll`): `head.mouse.<group>.mode`, `.deadzone`, `.smoothing`
- per axis (`pointer.x`, `pointer.y`, `scroll.x`, `scroll.y`): `head.mouse.<axis>.source`, `.neutral`, `.range.negative`, `.range.positive`, `.gain`, `.invert`

Profiles don't change. Switching profiles leaves the head mouse settings alone.

### 5. Controlling it from bindings
`TrackingWorker::execute` intercepts these commands before they reach `ScriptRunner`:
- `headmouse.toggle()`, `headmouse.enable()`, `headmouse.disable()`: these change `head.mouse.enabled` and save it, so the state is still there after a restart.
- `headmouse.hold()` / `headmouse.release()`: a clutch that freezes pointer and scroll while you reposition your head, like lifting a mouse off the desk. Not saved.
- `headmouse.recenter()`: captures the current value of every axis's source as its neutral and saves it.
- `headmouse.calibrate()`: starts the range calibration (as the Calibrate button does); the measured ranges are saved.

Rules:
- **Input simulation off:** `headmouse.recenter()` and `headmouse.calibrate()` still run, since they move nothing. Every other `headmouse.*` command is skipped, the same as other commands.
- **No mixing:** a command is either entirely `headmouse.*` calls or entirely `input_devices_simulator` script. A command that mixes the two is rejected with an error through `onError`, and none of it runs. The binding editor should show the same error when the command is entered.

**Head signals in bindings (optional, phase 6):** the head signals (`headRoll`, `headZ`, …) can also be offered to advanced binding expressions, since `Expression` already takes the variable list. For example, `headRoll > 15` presses a key when you tilt your head, and `headZ > 5` when you lean in. Simple bindings stay blendshape-only, because their 0–100 threshold slider doesn't fit degrees or centimetres.

### 6. Worker integration
- In `process()`, after `engine_.process`: if injection is on and the head mouse is active, call `simulator_->mouse().moveBy(dx, dy)` and `scroll(sx, sy)` directly. Don't send them through command strings, because at 30–60 Hz they would flood the command log.
- **When the face is lost:** stop moving and reset the filters and accumulators, but keep the neutral. Otherwise the pointer jumps when the face comes back.
- **On `pause()`:** reset the mapper (except the neutral). Switching profiles doesn't touch it.
- **Settings changes:** `AppController` passes new head mouse settings to the worker, which recompiles the sources. Changes made by `headmouse.*` commands go back from the worker to `AppController`, which saves them.
- **Calibrate:** for about 5 seconds the worker records each axis's furthest comfortable deviation from neutral in each direction (ignoring the top few percent of samples as outliers) and returns them as `range.negative`/`range.positive`. Injection from the head mouse is paused while it runs.
- **Click stabilisation (optional):** freeze the pointer for about 150 ms after a binding presses a mouse button. Making an expression to click moves your head a little. How the worker detects the press (checking the command for `mouse.press`/`mouse.click`, or a hook in `ScriptRunner`) gets decided in phase 5.

### 7. UI (QML)
- **Head mouse section in the settings**, with a Pointer tab and a Scroll tab. Each control is disabled while the UI is locked.
- **Presets** per group set all the sources and ranges at once:
  - *Head turn*: `headYaw` / `headPitch` (default)
  - *Head position*: `headX` / `headY`
  - *Nose point*: `noseX` / `noseY`
  - *Tilt and nod*: `headRoll` / `headPitch`
  - *Custom*: shows an editable source field per axis, with the list of signal names and the compile error, if any.
- Per group: mode, deadzone, smoothing. Per axis: source (preset or custom), sensitivity, invert, range (with a **Calibrate** button). Plus **Recenter**.
- **Live readout.** Each axis shows its raw source value and its normalised value. `LandmarkOverlay` draws a small puck for the pointer group's normalised vector inside the deadzone circle. Normalising first means the puck works the same for any source. Without it, tuning is guesswork.
- `FrameResult` carries the signals, and `AppController` passes them on to QML.

### 8. Testing
The head mouse has to be verifiable without anyone moving in front of a camera. Every layer gets automated tests: the math with made-up signals, the face tracking with images whose head pose is known, the whole app with a video file standing in for the camera, and how it feels with measurements. None of these tests turn on a webcam or move the real pointer, except the CI-only injection check in 8.5.

#### 8.1 Mapper (synthetic signals)
`tst_head_mouse.cpp`, driven by made-up signal sequences:
- every axis works with any source: `headYaw`, `headX`, `headRoll`, a blendshape, and a compound expression
- normalisation, including different negative and positive ranges
- radial deadzone, gain and invert
- the accumulators stay correct over many small steps, for pointer and for scroll
- total movement is the same at 15 fps and at 60 fps, in both relative and joystick mode
- reset when the face is lost, with the neutral kept
- a source that doesn't compile turns only its axis off
- calibration, including ignoring outliers
- clutch and click freeze

Also:
- Matrix → signals: check angles and translation against known matrices, including the sign conventions.
- `tst_settings`: the `head.mouse.*` keys survive a save and reload, and files without them still load.
- Command parsing: `headmouse.*` commands are recognised, and mixed commands are rejected.

#### 8.2 Face tracking (images with a known pose)
Checks that `FaceTracker` reports the right signals, with the right signs, for real face images.
- **2D transforms of a real photo:** start from one front-facing photo: `portrait.jpg` from MediaPipe's own test assets, downloaded at configure time by `cmake/FetchTestAssets.cmake` (not committed; its license should be confirmed before it's ever redistributed). Rotating, shifting and scaling it in software gives exact expected values for `headRoll`, `noseX/noseY`, `headX/headY` (approximately, through the camera model) and `headZ` (from scale). No 3D rendering is needed for these.
- **3D renders for yaw and pitch:** a flat photo can't be turned, so `tools/face-render` renders a 3D head scan with Qt Quick 3D (already in the Qt install) offscreen at any yaw, pitch, roll and position. Candidate model: the Lee Perry-Smith head scan (CC BY 3.0, as used in the three.js examples). Its license and attribution go next to the other third-party licenses.
  - **Prototype this first:** render a few poses and check that MediaPipe finds the face and that its pose matches the render. If the render doesn't look real enough to MediaPipe, fall back to a small set of real photos with labelled head angles, from a dataset whose license allows it.
  - **Result:** MediaPipe tracks the renders. `face-render` is a small CPU rasteriser (no GPU, same pixels everywhere), not Qt Quick 3D. With MediaPipe's 63° field of view and the scan at 5.5 cm per unit, the frontal render tracks at yaw/pitch/roll within 2° and 49.9 cm (set: 50). Single-axis turns of 20° measure 20–21°, pitch ±12° measures ±11°, moves of 5 cm measure 5.0 cm, and leaning in 10 cm measures 10.4 cm. Moving the head sideways also reads as about 4° of yaw (the camera sees it from an angle), which matters for the Head position preset.
- **Robustness variants:** add camera-like noise, blur, JPEG compression, dim lighting and an off-center face to the same images. Check that the signals stay within tolerance, and measure how much they jitter for a still head.
- `tst_face_signals` runs `FaceTracker` on these images and compares the signals with the known pose, with tolerances (for example ±3° for angles). Images are generated at test time or stored small in `tests/data`; generated sequences are cached in the build directory, not committed.
- `face-probe --image` prints all the head signals, for checking one image by hand.

#### 8.3 Whole app (video file instead of camera, recording mouse)
- **Frame source option:** `TrackingWorker` takes frames from a pluggable source. Besides the camera there's a file source (an image sequence or a video through `QMediaPlayer` → `QVideoSink`) used by tests and by a `--frames <path>` command-line option. Timestamps come from the file, so a test can play it at 15 or 60 fps.
- **Recording mouse:** the worker can be given an `input_devices_simulator::RecordingPlatform` in place of the real one. It records each event as text (`mouse moveBy 5 -3`, `mouse scroll 0 -1`) and injects nothing.
- **Head motion scripts:** `face-render` (and the 2D photo transforms) turn a short script into an image sequence, for example "neutral 0.5 s, yaw to +20° over 1 s, hold 1 s, back to 0° over 1 s", or "face disappears for 0.5 s". Each script states the expected result.
- `tst_tracking_worker` (label `tracking`) plays these sequences through the worker with input simulation **off**, so the input simulator is never created: it checks the movement the head mouse computes, the presets, face loss, head signals in bindings, `headmouse.recenter()`/`calibrate()` working with input off, `toggle()` not working with input off, and mixed commands being rejected.
- `tst_head_mouse_e2e` (label `headmouse-e2e`) turns input simulation on and checks the recorded events: the pointer moved right and up (screen y negative) by what the mapper computed, nothing came out with input off, roll scrolled, a binding toggled the head mouse on, the clutch held it, a click froze it, absolute mode placed it, and a mixed command sent nothing. Bindings that use `headmouse.*` commands are driven by head signals (for example `headRoll > 12`), since the renders can't change expression.
- The app itself, started with `QT_QPA_PLATFORM=offscreen`, `--frames` and the recording platform, checks that the settings flow from `AppController` to the worker and back (recenter, toggle saving).

#### 8.4 UI (Qt Quick Test)
`tst_head_mouse_qml` loads the head mouse settings section and checks:
- each preset fills in the expected sources and ranges
- a custom source with an error shows the compile error and turns only that axis off
- Recenter and Calibrate update the shown values
- every control is disabled while the UI is locked
- the live readout and the overlay puck follow a fed-in signal stream

#### 8.5 Feel (measurements and a simulated user)
`tools/head-mouse-bench` runs the mapper (and the e2e path from 8.3) on generated head movements and reports numbers, so defaults are tuned from data instead of by hand:
- **Jitter:** pointer movement while the head is still, using the noisy images from 8.2.
- **Lag:** delay between the head moving and the pointer following, at each smoothing setting.
- **Overshoot and settling** after a quick head movement.
- **Frame-rate independence:** the same motion at 15, 30 and 60 fps, with random dropped frames.
- **Simulated user:** a model of a person aiming at on-screen targets (minimum-jerk head movements with some tremor and correction). It reports time to target, error and overshoot (a Fitts's-law style test) for each mode, preset and setting.

The bench prints a table and writes a CSV; CI keeps the results as a build artifact so changes in feel show up between commits. Thresholds for jitter, lag and frame-rate independence are checked as tests; the rest is reported.

**Real injection, CI only:** on the Windows CI machine (never on a user's machine), one test sends real `SendInput` moves through the head mouse and reads back the cursor position with `GetCursorPos`. It checks the whole chain, and measures how "Enhance pointer precision" changes movement so the documentation states it accurately.

#### 8.6 Where the tests run
- **CI (GitHub Actions)** runs every test above on Windows, Linux and macOS. It is the main place they run, because nothing there acts on a real desktop.
- **Locally:** labels `core` (mapper, signals, settings, the bench's `--check`), `tracking` (MediaPipe on renders and photos, the worker with input off) and `qml` touch neither a camera nor the input simulator. `headmouse-e2e` (`tst_head_mouse_e2e` and `app_smoke`, the whole app offscreen with `--frames` and `--recording-input`) uses `RecordingPlatform`, so it's run or skipped as a group. The CI-only injection test (`ci-injection`) is skipped unless `GAME_FACE_CI_INJECTION=1`.

#### 8.7 What still needs a person
Comfort and fatigue over long sessions, real webcams in poor conditions, and how specific games react to the input. The measurements in 8.5 narrow these down, and a short session with a tester covers the rest.

## Phases
All done, including the optional ones; `headmouse-e2e` and `ci-injection` still need a first run (CI, or locally by the user).

1. [x] Signals: pose output in `FaceTracker`, `headX/Y/Z`, `noseX/Y`, `face-probe` printing them, and the matrix tests.
2. [x] Test images: the 2D photo transforms, the `face-render` prototype (check MediaPipe accepts the renders, else the labelled-photo fallback), and `tst_face_signals`.
3. [x] `HeadMouse` mapper with sources, normalisation, relative and joystick modes, and tests (8.1).
4. [x] Pluggable frame source and `--frames`, the recording mouse in the worker, and head motion scripts.
5. [x] Worker integration (pointer and scroll), calibration, the `headmouse.*` commands, the global settings, and `tst_head_mouse_e2e`.
6. [x] QML settings section with presets and custom sources, the live readout, the overlay puck, and `tst_head_mouse_qml`.
7. [x] `head-mouse-bench`, the simulated user, tuning the defaults from its numbers, CI artifacts, and the CI-only injection test.
8. [x] Optional: absolute mode, and tuning the click freeze.
9. [x] Optional: head signals in advanced binding expressions.

## Platform notes
- **Windows:** relative `SendInput` moves go through "Enhance pointer precision" (pointer acceleration), so the same head movement can move the pointer by different amounts. Document this in the user-facing help: turn the option off in Windows mouse settings for consistent movement. No compensation in code.
- **macOS:** needs the existing Accessibility permission.
- **Linux:** the uinput relative device works as it is.

## Decisions
1. Default pointer mode: relative. Scroll defaults to joystick.
2. Settings are global, not per profile.
3. Any signal, roll and head position included, can drive any output axis through its source expression. Defaults: yaw → pointer X, pitch → pointer Y, scroll off.
4. dt scales joystick velocity and the velocity used for relative-mode acceleration, never the relative movement itself.
5. When the face comes back, the saved neutral is kept. The user recenters or adjusts it in the settings.
6. `headmouse.toggle/enable/disable` save the enabled state.
7. `headmouse.recenter()` (and `headmouse.calibrate()`, added later) work even when input simulation is off.
8. Commands can't mix `headmouse.*` calls with other script. Mixed commands are rejected.
9. "Enhance pointer precision" is documented, not compensated for.
10. Each source is normalised by its range before smoothing, the deadzone and gain, so those settings mean the same thing for every source.
11. Signals are defined from the user's point of view. Mirroring is handled when the signals are computed, not per axis.
12. Every feature has to be testable without anyone moving in front of a camera: generated face images with a known pose, a video file in place of the camera, a recording mouse, and measured feel (section 8). Real injection is only tested in CI.
13. Relative mode's deadzone is a backlash, not a speed threshold (section 3, step 3).
14. Defaults, from `head-mouse-bench` (deadzone and smoothing swept at 1×, 2× and 3× the measured tracking noise): deadzone 0.06, smoothing 0.6. At the measured noise (yaw 0.15°, pitch 0.27° on a still head) the pointer doesn't drift and the simulated user hits every target in about 1.4 s (2.6 bits/s); at 3× it drifts 0.7 px/s and misses 6%.
