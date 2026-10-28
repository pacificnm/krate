# Klick: metronome instrument for the Krate rack.

add_library(klick_dsp STATIC
    dsp/Klick.h
    dsp/Klick.cpp
)
target_include_directories(klick_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(klick_dsp PUBLIC krate_core)
target_compile_options(klick_dsp PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(tst_klick tests/tst_klick.cpp)
target_compile_options(tst_klick PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_klick PRIVATE klick_dsp krate_testing Qt6::Test)
add_test(NAME tst_klick COMMAND tst_klick)
