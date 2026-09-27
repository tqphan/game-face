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

# Keyboard/mouse injection. Until the repository has a remote, a checkout next
# to this one is used; override with -DFETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR=<dir>
# or set IDS_GIT_REPOSITORY.
include(FetchContent)
set(IDS_GIT_REPOSITORY "" CACHE STRING "Git URL of input_devices_simulator")
set(IDS_GIT_TAG main CACHE STRING "Tag or commit of input_devices_simulator")
set(_ids_sibling ${PROJECT_SOURCE_DIR}/../input_devices_simulator)
if(NOT FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR AND NOT IDS_GIT_REPOSITORY)
    if(NOT EXISTS ${_ids_sibling}/CMakeLists.txt)
        message(FATAL_ERROR "input_devices_simulator not found next to game-face; "
                            "set IDS_GIT_REPOSITORY or FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR")
    endif()
    set(FETCHCONTENT_SOURCE_DIR_INPUT_DEVICES_SIMULATOR ${_ids_sibling})
endif()
FetchContent_Declare(input_devices_simulator
    GIT_REPOSITORY ${IDS_GIT_REPOSITORY}
    GIT_TAG ${IDS_GIT_TAG})
FetchContent_MakeAvailable(input_devices_simulator)

add_subdirectory(app/core)
add_subdirectory(app/ui)
add_subdirectory(tools/face-probe)
