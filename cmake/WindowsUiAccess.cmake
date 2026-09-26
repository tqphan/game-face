# Embeds packaging/windows/game-face.manifest (uiAccess="true") into Release builds only.
#
# Usage (after qt_add_executable):
#   include(WindowsUiAccess)
#   game_face_enable_uiaccess(game-face)
#
# /MANIFESTUAC:NO stops the linker from generating its own trustInfo
# (uiAccess='false'), which would otherwise conflict with the one in our manifest.
# The release workflow extracts the embedded manifest with mt.exe and fails if
# uiAccess="true" is missing, so a toolchain change can't silently drop it.

function(game_face_enable_uiaccess target)
    if(NOT MSVC)
        return()
    endif()

    set(manifest "${PROJECT_SOURCE_DIR}/packaging/windows/game-face.manifest")
    target_link_options(${target} PRIVATE
        "$<$<CONFIG:Release>:/MANIFESTUAC:NO>"
        "$<$<CONFIG:Release>:/MANIFESTINPUT:${manifest}>"
    )
    # Relink when the manifest changes.
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${manifest}")
endfunction()
