# Kobalt: monophonic synthesizer (instruments/kobalt/README.md).

set(KOBALT_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- kobalt_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(kobalt_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Filter.h
    dsp/Filter.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Voice.h
    dsp/Voice.cpp
)
# Included as "kobalt/dsp/...", "kobalt/app/...": instruments share file
# names (Engine.h, Params.h), and the rack builds against all of them.
target_include_directories(kobalt_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(kobalt_dsp PUBLIC krate_core krate_dsp)
target_compile_options(kobalt_dsp PRIVATE ${KOBALT_WARNINGS})

# --- kobalt-render: offline renderer and benchmark
add_executable(kobalt-render tools/render_main.cpp)
target_link_libraries(kobalt-render PRIVATE kobalt_dsp)
target_compile_options(kobalt-render PRIVATE ${KOBALT_WARNINGS})

# --- Tests
foreach(test tst_voice)
    add_executable(kobalt_${test} tests/${test}.cpp)
    target_compile_options(kobalt_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(kobalt_${test} PRIVATE kobalt_dsp krate_testing Qt6::Test)
    add_test(NAME kobalt_${test} COMMAND kobalt_${test})
endforeach()
