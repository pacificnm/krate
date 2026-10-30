# Klang: sampler (instruments/klang/README.md).

set(KLANG_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- klang_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(klang_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Pattern.h
    dsp/Sample.h
    dsp/Voice.h
    dsp/Voice.cpp
)
# Included as "klang/dsp/...", "klang/app/...": instruments share file names.
target_include_directories(klang_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(klang_dsp PUBLIC krate_core krate_dsp)
target_compile_options(klang_dsp PRIVATE ${KLANG_WARNINGS})

# --- klang-render: offline renderer and benchmark
add_executable(klang-render tools/render_main.cpp)
target_link_libraries(klang-render PRIVATE klang_dsp)
target_compile_options(klang-render PRIVATE ${KLANG_WARNINGS})

# --- Tests
foreach(test tst_pads tst_patterns)
    add_executable(klang_${test} tests/${test}.cpp)
    target_compile_options(klang_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(klang_${test} PRIVATE klang_dsp krate_testing Qt6::Test)
    add_test(NAME klang_${test} COMMAND klang_${test})
endforeach()
