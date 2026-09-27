cmake_minimum_required(VERSION 3.24)

project(game-face
    VERSION 0.1.0
    DESCRIPTION "Facial expressions to keyboard and mouse input"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

option(GAME_FACE_BUILD_TESTS "Build the core unit tests" ON)
if(GAME_FACE_BUILD_TESTS)
    enable_testing()
endif()

# The face landmarker model shipped with the Python app, reused as-is.
set(GAME_FACE_FACE_MODEL ${PROJECT_SOURCE_DIR}/src/assets/mediapipe/face_landmarker.task)
include(FetchMediaPipe)

# Keyboard/mouse injection. A checkout next to this one is used when present
# (turn off IDS_USE_SIBLING to ignore it); otherwise the pinned commit is fetched
# from GitHub. -DFETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR=<dir> overrides both.
include(FetchContent)
set(IDS_GIT_REPOSITORY "https://github.com/tqphan/input_devices_simulator.git"
    CACHE STRING "Git URL of input_devices_simulator")
set(IDS_GIT_TAG 7b3bb41e07109f6eb763eb1c3ad7cf462b2ded90
    CACHE STRING "Tag or commit of input_devices_simulator")
option(IDS_USE_SIBLING "Use ../input_devices_simulator when it exists" ON)
set(_ids_sibling ${PROJECT_SOURCE_DIR}/../input_devices_simulator)
if(NOT FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR
   AND IDS_USE_SIBLING AND EXISTS ${_ids_sibling}/CMakeLists.txt)
    set(FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR ${_ids_sibling})
endif()
if(FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR)
    message(STATUS "input_devices_simulator: ${FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR}")
elseif(NOT IDS_GIT_REPOSITORY)
    message(FATAL_ERROR "IDS_GIT_REPOSITORY is empty; set it or FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR")
else()
    message(STATUS "input_devices_simulator: ${IDS_GIT_REPOSITORY} @ ${IDS_GIT_TAG}")
endif()
FetchContent_Declare(input_devices_simulator
    GIT_REPOSITORY ${IDS_GIT_REPOSITORY}
    GIT_TAG ${IDS_GIT_TAG})
FetchContent_MakeAvailable(input_devices_simulator)

add_subdirectory(app/core)
add_subdirectory(app/ui)
add_subdirectory(tools/face-probe)
