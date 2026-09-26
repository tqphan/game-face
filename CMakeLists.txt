cmake_minimum_required(VERSION 3.24)

project(game-face
    VERSION 0.1.0
    DESCRIPTION "Facial expressions to keyboard and mouse input"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# The face landmarker model shipped with the Python app, reused as-is.
set(GAME_FACE_FACE_MODEL ${PROJECT_SOURCE_DIR}/src/assets/mediapipe/face_landmarker.task)
include(FetchMediaPipe)

add_subdirectory(app/core)
add_subdirectory(tools/face-probe)
