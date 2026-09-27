find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Multimedia Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
# The QML files live in qml/; generate a qmldir there too.
qt_policy(SET QTP0004 NEW)

qt_add_executable(game-face WIN32 MACOSX_BUNDLE main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(game-face
    URI GameFace
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ProfilePanel.qml
        qml/BindingList.qml
        qml/BindingCard.qml
        qml/TriggerEditor.qml
        qml/CommandField.qml
        qml/SettingsDialog.qml
        qml/NewProfileDialog.qml
        qml/BlendshapesDialog.qml
        qml/CloseDialog.qml
        qml/Theme.qml
        qml/AppButton.qml
        qml/AppDialogButtons.qml
        qml/AppDialog.qml
    SOURCES
        app_controller.h app_controller.cpp
        face_connections.h
        landmark_overlay.h landmark_overlay.cpp
        list_models.h list_models.cpp
        settings_object.h settings_object.cpp
        tracking_worker.h tracking_worker.cpp
)

qt_add_resources(game-face "images"
    PREFIX /game-face
    BASE ${PROJECT_SOURCE_DIR}/src/assets/images
    FILES ${PROJECT_SOURCE_DIR}/src/assets/images/icon.png
)

target_link_libraries(game-face PRIVATE
    game_face_core
    input_devices_simulator::input_devices_simulator
    Qt6::Gui Qt6::Multimedia Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

if(MSVC)
    target_compile_options(game-face PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(game-face PRIVATE -Wall -Wextra)
endif()

include(WindowsUiAccess)
game_face_enable_uiaccess(game-face)
game_face_deploy_mediapipe(game-face)
