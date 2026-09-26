find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
set(CMAKE_AUTOMOC ON)

# Frozen copies of the original app's JSON files.
set(test_data_dir ${CMAKE_CURRENT_SOURCE_DIR}/data)

foreach(name expression binding_engine profiles settings)
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE game_face_core Qt6::Core Qt6::Test)
    target_compile_definitions(tst_${name} PRIVATE GAME_FACE_TEST_DATA_DIR="${test_data_dir}")
    add_test(NAME ${name} COMMAND tst_${name})
    set_tests_properties(${name} PROPERTIES LABELS core)
endforeach()
