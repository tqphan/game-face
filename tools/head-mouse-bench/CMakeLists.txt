find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

# Measures the head mouse mapper (docs/head-mouse-plan.md, 8.5). Injects nothing.
qt_add_executable(head-mouse-bench main.cpp)
target_link_libraries(head-mouse-bench PRIVATE game_face_core Qt6::Core)

if(GAME_FACE_BUILD_TESTS)
    # The default settings must meet the bench's limits.
    add_test(NAME head_mouse_bench COMMAND head-mouse-bench --check)
    set_tests_properties(head_mouse_bench PROPERTIES LABELS core)
endif()
