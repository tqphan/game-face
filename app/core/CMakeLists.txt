# Plain C++ (no Qt): face tracking now; bindings and settings in phase 3.
add_library(game_face_core STATIC
    src/dynamic_library.cpp
    src/face_tracker.cpp
)
target_include_directories(game_face_core
    PUBLIC include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/mediapipe_c
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(game_face_core PRIVATE ${CMAKE_DL_LIBS})
endif()
if(MSVC)
    target_compile_options(game_face_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(game_face_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
