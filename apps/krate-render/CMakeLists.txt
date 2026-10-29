# krate-render: the rack (k303 + Klick, optionally K909, Kalliope and Krimson) rendered offline to WAV.
add_executable(krate-render main.cpp)
target_link_libraries(krate-render PRIVATE krate_rack k303_dsp klick_dsp k909_dsp kalliope_dsp krimson_dsp)
target_compile_options(krate-render PRIVATE -Wall -Wextra -Wpedantic -Werror)
