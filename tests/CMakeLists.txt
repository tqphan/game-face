# Tests that run the real face tracker, the tracking worker or the QML, on
# images and recorded head motion instead of a camera
# (docs/head-mouse-plan.md, section 8). Labels:
#   tracking       MediaPipe on rendered and transformed images
#   headmouse-e2e  the tracking worker or the whole app, with input recorded
#                  as text by input_devices_simulator's RecordingPlatform
#   qml            Qt Quick Test of the head mouse settings page
#   ci-injection   real pointer movement; CI only (GAME_FACE_CI_INJECTION=1)
find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Multimedia Test)
set(CMAKE_AUTOMOC ON)

qt_add_executable(tst_face_signals tst_face_signals.cpp)
target_link_libraries(tst_face_signals PRIVATE face_render game_face_core Qt6::Gui Qt6::Test)
target_compile_definitions(tst_face_signals PRIVATE GAME_FACE_TEST_ASSETS_DIR="${GAME_FACE_TEST_ASSETS_DIR}")
game_face_deploy_mediapipe(tst_face_signals)
add_test(NAME face_signals COMMAND tst_face_signals)
set_tests_properties(face_signals PROPERTIES LABELS tracking)

# The tracking worker with input simulation off: never creates the input simulator.
qt_add_executable(tst_tracking_worker tst_tracking_worker.cpp worker_harness.h)
target_link_libraries(tst_tracking_worker PRIVATE face_render game_face_tracking Qt6::Test)
target_compile_definitions(tst_tracking_worker PRIVATE GAME_FACE_TEST_ASSETS_DIR="${GAME_FACE_TEST_ASSETS_DIR}")
game_face_deploy_mediapipe(tst_tracking_worker)
add_test(NAME tracking_worker COMMAND tst_tracking_worker)
set_tests_properties(tracking_worker PROPERTIES LABELS tracking)

# Input on, recorded as text.
qt_add_executable(tst_head_mouse_e2e tst_head_mouse_e2e.cpp worker_harness.h)
target_link_libraries(tst_head_mouse_e2e PRIVATE face_render game_face_tracking Qt6::Test)
target_compile_definitions(tst_head_mouse_e2e PRIVATE GAME_FACE_TEST_ASSETS_DIR="${GAME_FACE_TEST_ASSETS_DIR}")
game_face_deploy_mediapipe(tst_head_mouse_e2e)
add_test(NAME head_mouse_e2e COMMAND tst_head_mouse_e2e)
set_tests_properties(head_mouse_e2e PROPERTIES LABELS headmouse-e2e)

# The whole app on recorded frames, offscreen, input printed instead of sent.
# Not with the uiAccess manifest: Windows won't start that unsigned, in place.
if(MSVC AND GAME_FACE_UIACCESS)
    message(STATUS "app_smoke skipped: configure with -DGAME_FACE_UIACCESS=OFF to run it")
else()
    add_test(NAME app_smoke
        COMMAND ${CMAKE_COMMAND}
            -DGAME_FACE=$<TARGET_FILE:game-face>
            -DFACE_RENDER=$<TARGET_FILE:face-render>
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/app_smoke
            -P ${CMAKE_CURRENT_SOURCE_DIR}/app_smoke.cmake)
    set_tests_properties(app_smoke PROPERTIES LABELS headmouse-e2e)
endif()

# The head mouse settings page, with stand-ins for App's objects.
find_package(Qt6 6.5 REQUIRED COMPONENTS QuickTest QuickControls2)
qt_add_executable(tst_head_mouse_qml tst_head_mouse_qml.cpp)
target_link_libraries(tst_head_mouse_qml PRIVATE Qt6::QuickTest Qt6::QuickControls2)
target_compile_definitions(tst_head_mouse_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME head_mouse_qml COMMAND tst_head_mouse_qml)
set_tests_properties(head_mouse_qml PROPERTIES LABELS qml ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# Real pointer movement; skips unless GAME_FACE_CI_INJECTION=1. CI only.
qt_add_executable(tst_injection_ci tst_injection_ci.cpp worker_harness.h)
target_link_libraries(tst_injection_ci PRIVATE face_render game_face_tracking Qt6::Test)
target_compile_definitions(tst_injection_ci PRIVATE GAME_FACE_TEST_ASSETS_DIR="${GAME_FACE_TEST_ASSETS_DIR}")
game_face_deploy_mediapipe(tst_injection_ci)
add_test(NAME injection_ci COMMAND tst_injection_ci)
set_tests_properties(injection_ci PROPERTIES LABELS ci-injection)
