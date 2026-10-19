# Head mouse: plan

Move the mouse pointer by moving your head, alongside the existing expression bindings.

## Starting point

- MediaPipe already returns a 4×4 head-pose matrix per face (`facial_transformation_matrixes` in [mediapipe_c_api.h](../third_party/mediapipe_c/mediapipe_c_api.h)), but [face_tracker.cpp](../app/core/src/face_tracker.cpp) turns it off (`output_facial_transformation_matrixes = false`), and `FaceFrame` only carries blendshapes and landmarks.
- `input_devices_simulator` has `Mouse::moveBy(int dx, int dy)` and `moveTo(x, y)` on every platform.
- `TrackingWorker::process` ([tracking_worker.cpp](../app/ui/tracking_worker.cpp)) is where it hooks in: it has the face result, the frame timestamp, the simulator and the injection gate.

## Design

### 1. Head pose in core (`FaceTracker`)
- Turn on `output_facial_transformation_matrixes`.
- Add `std::optional<HeadPose> pose` to `FaceFrame`. `HeadPose` is `{yaw, pitch, roll}` in degrees plus a translation, all taken from the matrix.
- Confirm the matrix's row-major or column-major layout against MediaPipe v1.0.0's `mediapipe/tasks/c/components/containers/matrix.h`, and write the answer down in `mediapipe_c_api.h`.
- Use the matrix, not a nose-tip landmark. The matrix is a true rotation. The landmark also moves when you lean or shift in your seat.

### 2. `HeadMouse` mapper in core (no Qt, testable)
Each frame it takes `(pose, t_ms)` and returns integer `(dx, dy)`:
1. **Neutral offset.** Subtract the calibrated neutral pose. "Recenter" captures it, and so does the first frame with a face.
2. **Smoothing.** Run yaw and pitch through a One Euro filter. That removes the jitter you get when holding still, without adding lag to fast movement.
3. **Deadzone, then a response curve, depending on the mode:**
   - **Relative (default).** Pointer movement = change in angle × gain, with acceleration. It feels like a trackpad, and it works in games that read raw input.
   - **Joystick.** Pointer velocity = offset from neutral × gain. Keep your head turned and the pointer keeps moving. Good for camera control in FPS games and for large screens.
   - **Absolute (later).** Angle maps directly to a screen position through `moveTo`. Desktop only, because games ignore absolute moves.
4. **Sub-pixel accumulator.** `moveBy` only takes whole pixels, so carry the fractional remainder to the next frame. Without it, slow movement stalls.
5. **Scale by frame time (dt).** Only one frame is in flight at a time, so frames get dropped. Scaling by dt keeps the speed the same when they do.
6. **Per-axis settings:** gain X/Y and invert X/Y, plus mirroring yaw, because the camera image is flipped.

### 3. Settings: per profile
- Add a `"headMouse": { enabled, mode, gainX, gainY, deadzone, smoothing, invertX, invertY }` block to `Profile`. A game profile and a desktop profile want different settings.
- `profiles.cpp` reads the block leniently (any missing field uses its default) and writes it with the other fields.

### 4. Controlling it from bindings
`TrackingWorker::execute` intercepts these commands before they reach `ScriptRunner`:
- `headmouse.toggle()`, `headmouse.enable()`, `headmouse.disable()`
- `headmouse.hold()` / `headmouse.release()`: a clutch that freezes the pointer while you reposition your head, like lifting a mouse off the desk.
- `headmouse.recenter()`

Any expression can then drive the head mouse. For example, a smile toggles it, raising your eyebrows works the clutch, and opening your jaw clicks. Clicking already works today with `mouse.press(...)`.

### 5. Worker integration
- In `process()`, after `engine_.process`: if injection is on and the head mouse is active, call `simulator_->mouse().moveBy(dx, dy)` directly. Don't send it through command strings, because at 30–60 Hz they would flood the command log.
- **When the face is lost:** stop moving and reset the filter and the accumulator. Otherwise the pointer jumps when the face comes back.
- **On `pause()` and `setProfile`:** reset the mapper.
- **Click stabilisation (optional):** freeze the pointer for about 150 ms after a binding presses a mouse button. Making an expression to click moves your head a little.

### 6. UI (QML)
- **Head mouse section in the profile panel.** Controls: enable, mode, sensitivity sliders, deadzone, smoothing, invert checkboxes, and a Recenter button. Each control is disabled while the UI is locked.
- **Live readout.** Show yaw and pitch as numbers. `LandmarkOverlay` draws a small puck showing the offset from neutral inside the deadzone circle. Without this, tuning the settings is guesswork.
- `FrameResult` carries the pose, and `AppController` passes it on to QML.

### 7. Testing (no camera, no injection)
- `tst_head_mouse.cpp`, driven by synthetic pose sequences:
  - deadzone, gain and invert
  - the accumulator stays correct over many small steps
  - total movement is the same at 15 fps and at 60 fps
  - reset when the face is lost
  - clutch and click freeze
- Matrix → angles: check against known rotation matrices.
- `tst_profiles`: the `headMouse` block survives a save and reload, and files without it still load.
- `face-probe --image` prints the pose. Run it on a photo of a turned head to check which direction is positive.
- How it feels with a live camera is tested by hand.

## Phases
1. [ ] Pose output in `FaceTracker`, `face-probe` printing it, and the matrix tests.
2. [ ] `HeadMouse` mapper (relative and joystick) with tests.
3. [ ] Worker integration, the `headmouse.*` commands, and the profile schema.
4. [ ] QML settings section and the overlay puck.
5. [ ] Optional: absolute mode, and tuning the click freeze.

## Platform notes
- **Windows:** relative `SendInput` moves go through "Enhance pointer precision" (pointer acceleration). Either document it or offer a linear option that compensates for it.
- **macOS:** needs the existing Accessibility permission.
- **Linux:** the uinput relative device works as it is.

## Open questions
1. Default mode: relative or joystick? It depends on whether the main use is games or desktop pointing.
2. Per-profile `headMouse` block, or one global setting?
3. Should tilting your head sideways (roll) do anything, such as scrolling, or stay unused for now?
