# Core logic, no GUI: face tracking, bindings, expressions, settings and
# profiles. Qt Core is used for JSON and file I/O only.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(game_face_core STATIC
    src/binding_engine.cpp
    src/blendshapes.cpp
    src/dynamic_library.cpp
    src/expression.cpp
    src/face_tracker.cpp
    src/json_util.cpp
    src/profiles.cpp
    src/settings.cpp
)
target_include_directories(game_face_core
    PUBLIC include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/mediapipe_c
)
target_link_libraries(game_face_core PRIVATE Qt6::Core)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(game_face_core PRIVATE ${CMAKE_DL_LIBS})
endif()
if(MSVC)
    target_compile_options(game_face_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(game_face_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(GAME_FACE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
