# Konnekt: a modular synthesizer, its own app (instruments/konnekt/README.md).

set(KONNEKT_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- konnekt_dsp: the engine and the modules; no Qt, no JACK
add_library(konnekt_dsp STATIC
    dsp/CoreModules.cpp
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Module.h
    dsp/Modules.h
    dsp/PatchSpec.h
    dsp/PatchSpec.cpp
)
target_include_directories(konnekt_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(konnekt_dsp PUBLIC krate_core krate_dsp)
target_compile_options(konnekt_dsp PRIVATE ${KONNEKT_WARNINGS})

# --- konnekt-render: offline renderer and benchmark
add_executable(konnekt-render tools/render_main.cpp)
target_link_libraries(konnekt-render PRIVATE konnekt_dsp)
target_compile_options(konnekt-render PRIVATE ${KONNEKT_WARNINGS})

# --- Tests
foreach(test tst_engine)
    add_executable(konnekt_${test} tests/${test}.cpp)
    target_compile_options(konnekt_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(konnekt_${test} PRIVATE konnekt_dsp krate_testing Qt6::Test)
    add_test(NAME konnekt_${test} COMMAND konnekt_${test})
endforeach()
