cmake_minimum_required(VERSION 3.24)
project(krate VERSION 0.1 LANGUAGES CXX)

# Krate: a suite of instruments and a rack that plays them together
# (docs/SHARED_FOUNDATIONS.MD). shared/ holds everything instrument-neutral,
# instruments/<name>/ each instrument, apps/<name>/ the thin executables.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(KRATE_SANITIZE "Build with ASan/UBSan" ON)
else()
    option(KRATE_SANITIZE "Build with ASan/UBSan" OFF)
endif()
if(KRATE_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
qt_standard_project_setup(REQUIRES 6.8)

# All QML modules land side by side, so tooling (qmllint) finds Krate from K303.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

enable_testing()

add_subdirectory(shared)
add_subdirectory(instruments/k303)
add_subdirectory(instruments/klick)
add_subdirectory(instruments/k909)
add_subdirectory(instruments/kalliope)
add_subdirectory(instruments/krimson)
add_subdirectory(apps/k303)
add_subdirectory(apps/k909)
add_subdirectory(apps/kalliope)
add_subdirectory(apps/krimson)
add_subdirectory(apps/krate-render)
add_subdirectory(apps/krate)
add_subdirectory(apps/song-dump)
