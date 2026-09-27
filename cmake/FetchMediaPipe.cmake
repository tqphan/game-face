# Downloads the pinned MediaPipe wheel from PyPI for the target platform,
# checks its SHA-256, and extracts libmediapipe (the Tasks C API library).
#
# Sets:
#   MEDIAPIPE_LIBRARY   full path to libmediapipe.{dll,so,dylib}
#
# Offline builds: set GAME_FACE_MEDIAPIPE_WHEEL to a local copy of the wheel
# (its hash is still checked).
#
# When bumping MEDIAPIPE_VERSION, update the hashes below from
# https://pypi.org/pypi/mediapipe/<version>/json and re-check
# third_party/mediapipe_c/mediapipe_c_api.h against the new ctypes bindings.

set(MEDIAPIPE_VERSION 1.0.1)
set(_mp_base https://files.pythonhosted.org/packages)

if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(_mp_path 49/47/8a901eade7352051ae4d7b0b070ad8a84bde9cbf3092afd69088982842bb)
    set(_mp_tag win_arm64)
    set(_mp_sha256 4bbbb3838a99f7fdcd3cb0e071560120df45ddc48c1ad097ef1457f8600e0e77)
    set(_mp_lib libmediapipe.dll)
elseif(WIN32)
    set(_mp_path 22/71/42365b0aec2a96dfbeb3441220fe8dccd9a833f36adfecf3aa9f211c449b)
    set(_mp_tag win_amd64)
    set(_mp_sha256 96dc9de6bd04a6315ef424fda5c48e0929f2d78317295e75bc32c0bceeab517b)
    set(_mp_lib libmediapipe.dll)
elseif(APPLE)
    # PyPI has no x86_64 macOS wheel, so Intel Macs are not supported.
    if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        message(FATAL_ERROR "MediaPipe ${MEDIAPIPE_VERSION} only ships an arm64 macOS library; "
                            "set CMAKE_OSX_ARCHITECTURES=arm64.")
    endif()
    set(_mp_path 18/56/911762884caba685dc8156d0136c58196a228c2b447023cfa0cfdb32f6c5)
    set(_mp_tag macosx_11_0_arm64)
    set(_mp_sha256 0a9fb67957f7d28e84f485e9c6716a43367b3f6f07170f31c3f72cac1addd031)
    set(_mp_lib libmediapipe.dylib)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(_mp_path 16/9d/515c6ebc98db21484b2b93b7403464d08fb7861b6430752801bc75d29372)
    set(_mp_tag manylinux_2_28_aarch64)
    set(_mp_sha256 d6050e773dc6698eb86324090f61af1ccab28a6f1e9f77d98fe0611cef707997)
    set(_mp_lib libmediapipe.so)
else()
    set(_mp_path 2a/58/bdd5bada89d7a132375df05e962bf702c148b47043dca98d820d9395152b)
    set(_mp_tag manylinux_2_28_x86_64)
    set(_mp_sha256 121522251afc3c135e4b7b0c341dd5e050ad1ec87631127484f3c389ae385044)
    set(_mp_lib libmediapipe.so)
endif()

set(_mp_wheel_name mediapipe-${MEDIAPIPE_VERSION}-py3-none-${_mp_tag}.whl)
set(_mp_dir ${CMAKE_BINARY_DIR}/_deps/mediapipe-${MEDIAPIPE_VERSION}-${_mp_tag})
set(MEDIAPIPE_LIBRARY ${_mp_dir}/mediapipe/tasks/c/${_mp_lib})

if(NOT EXISTS ${MEDIAPIPE_LIBRARY})
    if(GAME_FACE_MEDIAPIPE_WHEEL)
        set(_mp_wheel ${GAME_FACE_MEDIAPIPE_WHEEL})
        file(SHA256 ${_mp_wheel} _mp_actual)
        if(NOT _mp_actual STREQUAL _mp_sha256)
            message(FATAL_ERROR "${_mp_wheel}: SHA-256 ${_mp_actual} does not match ${_mp_sha256}")
        endif()
    else()
        set(_mp_wheel ${_mp_dir}/${_mp_wheel_name})
        message(STATUS "Downloading ${_mp_wheel_name}")
        file(DOWNLOAD ${_mp_base}/${_mp_path}/${_mp_wheel_name} ${_mp_wheel}
            EXPECTED_HASH SHA256=${_mp_sha256}
            TLS_VERIFY ON
            STATUS _mp_status)
        list(GET _mp_status 0 _mp_code)
        if(NOT _mp_code EQUAL 0)
            message(FATAL_ERROR "Downloading ${_mp_wheel_name} failed: ${_mp_status}")
        endif()
    endif()

    file(ARCHIVE_EXTRACT INPUT ${_mp_wheel} DESTINATION ${_mp_dir}
        PATTERNS "mediapipe/tasks/c/${_mp_lib}")
    if(NOT EXISTS ${MEDIAPIPE_LIBRARY})
        message(FATAL_ERROR "${_mp_lib} not found in ${_mp_wheel_name}")
    endif()
    file(REMOVE ${_mp_dir}/${_mp_wheel_name})
endif()

message(STATUS "MediaPipe ${MEDIAPIPE_VERSION}: ${MEDIAPIPE_LIBRARY}")

# Copies libmediapipe and the face landmarker model next to a target's binary,
# or into Contents/Frameworks and Contents/Resources of a macOS app bundle.
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEDIAPIPE_LIBRARY} ${lib_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GAME_FACE_FACE_MODEL} ${model_dir}
        VERBATIM)
endfunction()

# Installs libmediapipe and the model where game_face_deploy_mediapipe puts
# them in the build tree. For a bundle, call after install(TARGETS ... BUNDLE).
function(game_face_install_mediapipe target)
    get_target_property(bundle ${target} MACOSX_BUNDLE)
    if(APPLE AND bundle)
        set(contents ${target}.app/Contents)
        install(FILES ${MEDIAPIPE_LIBRARY} DESTINATION ${contents}/Frameworks)
        install(FILES ${GAME_FACE_FACE_MODEL} DESTINATION ${contents}/Resources)
    else()
        install(FILES ${MEDIAPIPE_LIBRARY} ${GAME_FACE_FACE_MODEL}
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endfunction()
