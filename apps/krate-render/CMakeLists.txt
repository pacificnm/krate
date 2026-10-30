# krate-render: the rack (k303 + Klick, optionally K909, Kalliope, Krimson and Kobalt), or a song through them all rendered offline to WAV.
add_executable(krate-render main.cpp)
target_link_libraries(krate-render PRIVATE krate_rack krate_song k303_dsp klick_dsp k909_dsp kalliope_dsp krimson_dsp kobalt_dsp)
target_compile_options(krate-render PRIVATE -Wall -Wextra -Wpedantic -Werror)
