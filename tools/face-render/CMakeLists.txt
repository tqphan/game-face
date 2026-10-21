find_package(Qt6 6.5 REQUIRED COMPONENTS Gui)

# The renderer, shared by the face-render tool and the tests.
add_library(face_render STATIC face_render.h face_render.cpp)
target_include_directories(face_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(face_render PUBLIC game_face_core Qt6::Gui)

qt_add_executable(face-render main.cpp)
target_link_libraries(face-render PRIVATE face_render)
target_compile_definitions(face-render PRIVATE GAME_FACE_TEST_ASSETS_DIR="${GAME_FACE_TEST_ASSETS_DIR}")
