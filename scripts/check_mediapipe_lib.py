"""Checks a libmediapipe build for Google's usage logger and for the C API
functions game-face loads.

    python scripts/check_mediapipe_lib.py path/to/libmediapipe.{so,dll,dylib}

Fails when the file contains any trace of the Clearcut logger that the PyPI
wheels ship from 0.10.35 on, or when a function face_tracker.cpp resolves is
missing. The scan works on raw bytes, so it runs the same on every platform;
the platform's own tools (nm, dumpbin) cover imports in the workflow.
"""

import sys

FORBIDDEN = [
    b"clearcut",
    b"play.googleapis.com",
    b"googleapis.com/log",
]

# Keep in sync with the symbols app/core/src/face_tracker.cpp resolves.
REQUIRED = [
    b"MpFaceLandmarkerCreate",
    b"MpFaceLandmarkerDetectImage",
    b"MpFaceLandmarkerDetectForVideo",
    b"MpFaceLandmarkerCloseResult",
    b"MpFaceLandmarkerClose",
    b"MpImageCreateFromUint8Data",
    b"MpImageFree",
    b"MpErrorFree",
]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    lowered = data.lower()

    failed = False
    for s in FORBIDDEN:
        if s in lowered:
            print(f"FOUND telemetry string: {s.decode()}")
            failed = True
    for s in REQUIRED:
        if s not in data:
            print(f"MISSING C API function: {s.decode()}")
            failed = True

    if failed:
        sys.exit(1)
    print(f"{sys.argv[1]}: no telemetry strings, all {len(REQUIRED)} C API functions present")


if __name__ == "__main__":
    main()
