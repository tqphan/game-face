find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Multimedia)

qt_add_executable(face-probe main.cpp)
target_link_libraries(face-probe PRIVATE game_face_core Qt6::Gui Qt6::Multimedia)
game_face_deploy_mediapipe(face-probe)
