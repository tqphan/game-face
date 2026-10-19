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
        qml/SettingsWindow.qml
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
        sound_player.h sound_player.cpp
        tracking_worker.h tracking_worker.cpp
)

qt_add_resources(game-face "images"
    PREFIX /game-face
    BASE ${PROJECT_SOURCE_DIR}/assets
    FILES
        ${PROJECT_SOURCE_DIR}/assets/icon.png
        ${PROJECT_SOURCE_DIR}/assets/sounds/click1.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/click3.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/click5.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/mouseclick1.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/mouserelease1.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/rollover2.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/rollover4.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/switch12.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/switch13.wav
        ${PROJECT_SOURCE_DIR}/assets/sounds/switch7.wav
)

if(WIN32)
    # The exe's own icon, used by the taskbar, Explorer, shortcuts and
    # Add/Remove Programs (setWindowIcon only covers the window).
    # scripts/generate_windows_icon.ps1 makes the .ico from icon.png.
    set(_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/game-face-icon.rc")
    file(WRITE "${_icon_rc}" "IDI_ICON1 ICON \"${PROJECT_SOURCE_DIR}/packaging/windows/game-face.ico\"\n")
    target_sources(game-face PRIVATE "${_icon_rc}")
endif()

# The project version (top-level CMakeLists.txt), shown next to the app name.
target_compile_definitions(game-face PRIVATE GAME_FACE_VERSION="${PROJECT_VERSION}")

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

# `cmake --install` produces a self-contained app: the executable (or bundle),
# libmediapipe, the model, and the Qt libraries, plugins and QML modules it uses.
include(GNUInstallDirs)
install(TARGETS game-face
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
game_face_install_mediapipe(game-face)
qt_generate_deploy_qml_app_script(TARGET game-face
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS)
install(SCRIPT ${deploy_script})
