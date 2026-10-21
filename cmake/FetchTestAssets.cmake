# Downloads the images and 3D head the face tracking tests render and track
# (docs/head-mouse-plan.md, section 8). They're only used by tests and tools,
# never shipped, and not committed: each is fetched once into the build tree
# and checked against its SHA-256.
#
#   LeePerrySmith.glb, Map-COL.jpg  "Infinite, 3D Head Scan" by Lee Perry-Smith,
#       CC BY 3.0 (https://creativecommons.org/licenses/by/3.0/), based on a
#       work at www.triplegangers.com; the copy in the three.js examples.
#   portrait.jpg  a test image from MediaPipe's test assets (the face
#       landmarker's own tests use it).
#
# Sets GAME_FACE_TEST_ASSETS_DIR. When a download fails (offline), the build
# goes on and the tests that need the file skip themselves.

set(_ta_dir ${CMAKE_BINARY_DIR}/_deps/test-assets)
set(_ta_three https://raw.githubusercontent.com/mrdoob/three.js/r160/examples/models/gltf/LeePerrySmith)
set(_ta_files
    "LeePerrySmith.glb|${_ta_three}/LeePerrySmith.glb|402b8a8ac9f03232e6d64b5962929703a069daf99d3c49ac8eb0e48bedc9c576"
    "Map-COL.jpg|${_ta_three}/Map-COL.jpg|e976d73b31407f8d0967412bf468019ed26a5d5a32cf5811aabff7e816458a65"
    "LeePerrySmith_License.txt|${_ta_three}/LeePerrySmith_License.txt|"
    "portrait.jpg|https://storage.googleapis.com/mediapipe-assets/portrait.jpg|a6f11efaa834706db23f275b6115058fa87fc7f14362681e6abe14e82749de3e"
)

foreach(_ta_entry IN LISTS _ta_files)
    string(REPLACE "|" ";" _ta_parts "${_ta_entry}")
    list(GET _ta_parts 0 _ta_name)
    list(GET _ta_parts 1 _ta_url)
    list(LENGTH _ta_parts _ta_count)
    set(_ta_hash "")
    if(_ta_count GREATER 2)
        list(GET _ta_parts 2 _ta_hash)
    endif()
    set(_ta_file ${_ta_dir}/${_ta_name})
    if(EXISTS ${_ta_file})
        continue()
    endif()
    message(STATUS "Downloading test asset ${_ta_name}")
    if(_ta_hash)
        file(DOWNLOAD ${_ta_url} ${_ta_file}.part EXPECTED_HASH SHA256=${_ta_hash}
            TLS_VERIFY ON STATUS _ta_status)
    else()
        file(DOWNLOAD ${_ta_url} ${_ta_file}.part TLS_VERIFY ON STATUS _ta_status)
    endif()
    list(GET _ta_status 0 _ta_code)
    if(_ta_code EQUAL 0)
        file(RENAME ${_ta_file}.part ${_ta_file})
    else()
        file(REMOVE ${_ta_file}.part)
        message(WARNING "Could not download test asset ${_ta_name} (${_ta_status}); "
                        "the tests that need it will be skipped.")
    endif()
endforeach()

set(GAME_FACE_TEST_ASSETS_DIR ${_ta_dir})
