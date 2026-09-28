# Downloads libmediapipe (the Tasks C API library) for the target platform from
# this repository's GitHub Release, checks the zip's SHA-256, and extracts it.
# The release is built by .github/workflows/mediapipe.yml from MediaPipe's
# open-source tree, without the usage logger in PyPI's mediapipe wheels.
#
# Sets:
#   MEDIAPIPE_LIBRARY        full path to libmediapipe.{dll,so,dylib}
#   MEDIAPIPE_RUNTIME_FILES  the libraries that ship next to the app: libmediapipe,
#                            plus opencv_world<ver>.dll on Windows (Linux and macOS
#                            link OpenCV statically)
#   MEDIAPIPE_LICENSE_FILES  MediaPipe's and OpenCV's licenses (both Apache-2.0)
#
# Offline builds: set GAME_FACE_MEDIAPIPE_ZIP to a local copy of the zip (its
# hash is still checked).
#
# Updating MediaPipe (or rebuilding the same version):
#   1. In .github/workflows/mediapipe.yml, set MEDIAPIPE_REF (the default of
#      env.MEDIAPIPE_REF) to the MediaPipe tag, and set MEDIAPIPE_VERSION below
#      to the same tag; the zip names contain it.
#   2. Push a commit with "[mediapipe release]" in its message. The workflow
#      builds every platform and, if all pass their checks, publishes a GitHub
#      Release tagged mediapipe-<tag>-<short commit>.
#   3. Set MEDIAPIPE_RELEASE below to that release's tag and each _mp_sha256 to
#      the matching line of its SHA256SUMS.
#   4. If the version changed, re-check third_party/mediapipe_c/mediapipe_c_api.h
#      against that tag's mediapipe/tasks/c headers (struct layouts and function
#      signatures), then test tracking with the new library.

set(MEDIAPIPE_VERSION v1.0.0)
set(MEDIAPIPE_RELEASE mediapipe-v1.0.0-ec499d2)
set(_mp_base https://github.com/tqphan/game-face/releases/download/${MEDIAPIPE_RELEASE})

if(WIN32)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
        message(FATAL_ERROR "libmediapipe is built for x86_64 Windows only, not ${CMAKE_SYSTEM_PROCESSOR}.")
    endif()
    set(_mp_platform windows-x86_64)
    set(_mp_sha256 e8177466c2139f7d5b62226fd9f33025668405f0e880611d91e15475e30aefcf)
    set(_mp_lib libmediapipe.dll)
elseif(APPLE)
    if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        message(FATAL_ERROR "libmediapipe is built for arm64 macOS only; set CMAKE_OSX_ARCHITECTURES=arm64.")
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        message(FATAL_ERROR "libmediapipe is built for arm64 macOS only (Apple silicon), not ${CMAKE_SYSTEM_PROCESSOR}.")
    endif()
    set(_mp_platform macos-arm64)
    set(_mp_sha256 8aee7aacf4ff6ec2a7424254fee4de4109f2e128fee1724248a495aa4abe98fc)
    set(_mp_lib libmediapipe.dylib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    # Built on Ubuntu 24.04, so it needs glibc 2.39 or newer.
    set(_mp_platform linux-x86_64)
    set(_mp_sha256 13e2a730dcbaf94f00db27277b0d6c28ac231a6e99bcc7140ef671c69a4666d0)
    set(_mp_lib libmediapipe.so)
else()
    message(FATAL_ERROR "libmediapipe is built for Windows x86_64, Linux x86_64 and macOS arm64 only, "
                        "not ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}.")
endif()

set(_mp_zip_name libmediapipe-${MEDIAPIPE_VERSION}-${_mp_platform}.zip)
# MEDIAPIPE_RELEASE starts with "mediapipe-", which ci.yml's cache path matches.
set(_mp_dir ${CMAKE_BINARY_DIR}/_deps/${MEDIAPIPE_RELEASE}-${_mp_platform})
set(MEDIAPIPE_LIBRARY ${_mp_dir}/${_mp_lib})

if(NOT EXISTS ${MEDIAPIPE_LIBRARY})
    if(GAME_FACE_MEDIAPIPE_ZIP)
        set(_mp_zip ${GAME_FACE_MEDIAPIPE_ZIP})
        file(SHA256 ${_mp_zip} _mp_actual)
        if(NOT _mp_actual STREQUAL _mp_sha256)
            message(FATAL_ERROR "${_mp_zip}: SHA-256 ${_mp_actual} does not match ${_mp_sha256}")
        endif()
    else()
        set(_mp_zip ${CMAKE_BINARY_DIR}/_deps/${_mp_zip_name})
        message(STATUS "Downloading ${_mp_zip_name} (${MEDIAPIPE_RELEASE})")
        file(DOWNLOAD ${_mp_base}/${_mp_zip_name} ${_mp_zip}
            EXPECTED_HASH SHA256=${_mp_sha256}
            TLS_VERIFY ON
            STATUS _mp_status)
        list(GET _mp_status 0 _mp_code)
        if(NOT _mp_code EQUAL 0)
            message(FATAL_ERROR "Downloading ${_mp_zip_name} failed: ${_mp_status}")
        endif()
    endif()

    file(ARCHIVE_EXTRACT INPUT ${_mp_zip} DESTINATION ${_mp_dir})
    if(NOT EXISTS ${MEDIAPIPE_LIBRARY})
        message(FATAL_ERROR "${_mp_lib} not found in ${_mp_zip_name}")
    endif()
    if(NOT GAME_FACE_MEDIAPIPE_ZIP)
        file(REMOVE ${_mp_zip})
    endif()
endif()

file(GLOB MEDIAPIPE_RUNTIME_FILES ${_mp_dir}/*.dll ${_mp_dir}/*.so ${_mp_dir}/*.dylib)
set(MEDIAPIPE_LICENSE_FILES ${_mp_dir}/LICENSE.mediapipe ${_mp_dir}/LICENSE.opencv)

message(STATUS "MediaPipe ${MEDIAPIPE_VERSION} (${MEDIAPIPE_RELEASE}): ${MEDIAPIPE_LIBRARY}")

# Copies libmediapipe, the libraries it needs, and the face landmarker model
# next to a target's binary, or into Contents/Frameworks and Contents/Resources
# of a macOS app bundle.
function(game_face_deploy_mediapipe target)
    get_target_property(bundle ${target} MACOSX_BUNDLE)
    if(APPLE AND bundle)
        set(lib_dir $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks)
        set(model_dir $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources)
    else()
        set(lib_dir $<TARGET_FILE_DIR:${target}>)
        set(model_dir $<TARGET_FILE_DIR:${target}>)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${lib_dir} ${model_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEDIAPIPE_RUNTIME_FILES} ${lib_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GAME_FACE_FACE_MODEL} ${model_dir}
        VERBATIM)
endfunction()

# Installs what game_face_deploy_mediapipe copies, plus the licenses. For a
# bundle, call after install(TARGETS ... BUNDLE).
function(game_face_install_mediapipe target)
    get_target_property(bundle ${target} MACOSX_BUNDLE)
    if(APPLE AND bundle)
        set(contents ${target}.app/Contents)
        install(FILES ${MEDIAPIPE_RUNTIME_FILES} DESTINATION ${contents}/Frameworks)
        install(FILES ${GAME_FACE_FACE_MODEL} DESTINATION ${contents}/Resources)
        install(FILES ${MEDIAPIPE_LICENSE_FILES} DESTINATION ${contents}/Resources/licenses)
    else()
        install(FILES ${MEDIAPIPE_RUNTIME_FILES} ${GAME_FACE_FACE_MODEL}
            DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES ${MEDIAPIPE_LICENSE_FILES}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/game-face)
    endif()
endfunction()
