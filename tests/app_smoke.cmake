# The whole app on recorded frames (docs/head-mouse-plan.md, 8.3): renders a
# head turn with face-render, runs game-face offscreen on it with a fresh
# config that turns the head mouse and input simulation on, and checks the
# pointer movement it printed. --recording-input prints input instead of
# sending it. Run by ctest (label headmouse-e2e) with:
#   -DGAME_FACE=<game-face executable> -DFACE_RENDER=<face-render executable>
#   -DWORK_DIR=<scratch folder>

foreach(var GAME_FACE FACE_RENDER WORK_DIR)
    if(NOT ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR}/config)
file(WRITE ${WORK_DIR}/turn.json
    [=[{"fps": 15, "steps": [{"hold": 0.5}, {"to": {"yaw": 20}, "over": 1.0}, {"hold": 0.5}]}]=])
file(WRITE ${WORK_DIR}/config/settings.json [=[{
    "allow.input.simulation": true,
    "head.mouse.enabled": true
}]=])

execute_process(
    COMMAND ${FACE_RENDER} --motion ${WORK_DIR}/turn.json --out-dir ${WORK_DIR}/frames
    RESULT_VARIABLE rendered)
if(NOT rendered EQUAL 0)
    message(FATAL_ERROR "face-render failed (${rendered})")
endif()

set(ENV{QT_QPA_PLATFORM} offscreen)
execute_process(
    COMMAND ${GAME_FACE} --config ${WORK_DIR}/config --frames ${WORK_DIR}/frames
            --start-tracking --quit-when-frames-end --recording-input
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE exited
    TIMEOUT 120)
if(NOT exited EQUAL 0)
    message(FATAL_ERROR "game-face exited with ${exited}\n${errors}")
endif()

# input: mouse moveBy <dx> <dy>
string(REGEX MATCHALL "input: mouse moveBy -?[0-9]+ -?[0-9]+" moves "${output}")
set(total_dx 0)
foreach(move IN LISTS moves)
    string(REGEX REPLACE "input: mouse moveBy (-?[0-9]+) .*" "\\1" dx "${move}")
    math(EXPR total_dx "${total_dx} + ${dx}")
endforeach()
list(LENGTH moves count)
message(STATUS "${count} moves, ${total_dx} px to the right")
if(total_dx LESS 150)
    message(FATAL_ERROR "expected the pointer to move right at least 150 px; got ${total_dx}\n${output}\n${errors}")
endif()

# The neutral the head mouse captured was saved.
file(READ ${WORK_DIR}/config/settings.json saved)
if(NOT saved MATCHES "\"head.mouse.pointer.x.neutral\": -?[0-9]")
    message(FATAL_ERROR "the captured neutral wasn't saved:\n${saved}")
endif()
