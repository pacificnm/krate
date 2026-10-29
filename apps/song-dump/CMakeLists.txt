# song-dump: lists what is in a .gp tab file and a .mid file (docs/SONG_PLAYER.md, S0).
find_package(ZLIB REQUIRED)
add_executable(song-dump main.cpp)
target_link_libraries(song-dump PRIVATE krate_song Qt6::Core ZLIB::ZLIB)
target_compile_options(song-dump PRIVATE -Wall -Wextra -Wpedantic -Werror)
