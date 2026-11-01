# krate-render: the rack (Korrode + Klick, optionally Kessel, Kalliope, Krimson and Kobalt), or a song through them all rendered offline to WAV.
add_executable(krate-render main.cpp)
target_link_libraries(krate-render PRIVATE krate_rack krate_song korrode_dsp klick_dsp kessel_dsp kalliope_dsp krimson_dsp kobalt_dsp klang_dsp)
target_compile_options(krate-render PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Every instrument repeats itself after prepare (an export's passes, docs/DAW.md D6).
add_executable(tst_repeatable tst_repeatable.cpp)
target_link_libraries(tst_repeatable PRIVATE korrode_dsp kessel_dsp kalliope_dsp krimson_dsp kobalt_dsp klang_dsp Qt6::Test)
add_test(NAME tst_repeatable COMMAND tst_repeatable)
