cmake_minimum_required(VERSION 3.24)
project(k303 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(K303_SANITIZE "Build with ASan/UBSan" ON)
else()
    option(K303_SANITIZE "Build with ASan/UBSan" OFF)
endif()
if(K303_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
qt_standard_project_setup(REQUIRES 6.8)

# --- DSP core: no Qt, no JACK -------------------------------------------------
add_library(k303_dsp STATIC
    src/dsp/Denormals.h
    src/dsp/DiodeLadder.h
    src/dsp/DiodeLadder.cpp
    src/dsp/Engine.h
    src/dsp/Engine.cpp
    src/dsp/Oscillator.h
    src/dsp/HalfbandDecimator.h
    src/dsp/Oscillator.cpp
    src/dsp/Params.h
    src/dsp/Params.cpp
    src/dsp/ParamStore.h
    src/dsp/Sequencer.h
    src/dsp/Sequencer.cpp
    src/dsp/SpscQueue.h
    src/dsp/Voice.h
    src/dsp/Voice.cpp
)
target_include_directories(k303_dsp PUBLIC src)
target_compile_options(k303_dsp PRIVATE -Wall -Wextra -Wpedantic -Werror)

# --- JACK host -----------------------------------------------------------------
add_library(k303_jack STATIC
    src/jack/JackClient.h
    src/jack/JackClient.cpp
)
target_link_libraries(k303_jack PUBLIC k303_dsp PkgConfig::JACK)
target_compile_options(k303_jack PRIVATE -Wall -Wextra -Wpedantic -Werror)

# --- Application ---------------------------------------------------------------
qt_add_executable(k303
    src/main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(k303
    URI K303
    VERSION 1.0
    # Import scanning only matters for static Qt; with Debian's shared Qt it just
    # warns that plugin link targets don't exist.
    NO_IMPORT_SCAN
    SOURCES
        src/app/PatternModel.h
        src/app/PatternModel.cpp
        src/app/Persistence.h
        src/app/Persistence.cpp
        src/app/SynthController.h
        src/app/SynthController.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
        qml/Theme.qml
        qml/PanelLabel.qml
        qml/Led.qml
        qml/Knob.qml
        qml/PanelButton.qml
        qml/LcdField.qml
        qml/WaveSwitch.qml
        qml/StepRow.qml
        qml/PianoKeys.qml
)

target_include_directories(k303 PRIVATE src/app) # generated QML type registration includes headers by bare name
target_compile_options(k303 PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(k303 PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(k303 PRIVATE
    k303_jack
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

# --- Offline renderer ------------------------------------------------------------
add_executable(k303-render tools/render_main.cpp)
target_link_libraries(k303-render PRIVATE k303_dsp)
target_compile_options(k303-render PRIVATE -Wall -Wextra -Wpedantic -Werror)

# --- Tests ---------------------------------------------------------------------
enable_testing()

qt_add_executable(tst_params tests/tst_params.cpp)
target_compile_options(tst_params PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_params PRIVATE k303_dsp Qt6::Test)
add_test(NAME tst_params COMMAND tst_params)

qt_add_executable(tst_dsp tests/tst_dsp.cpp)
target_compile_options(tst_dsp PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_dsp PRIVATE k303_dsp Qt6::Test)
add_test(NAME tst_dsp COMMAND tst_dsp)

qt_add_executable(tst_sequencer tests/tst_sequencer.cpp)
target_compile_options(tst_sequencer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_sequencer PRIVATE k303_dsp Qt6::Test)
add_test(NAME tst_sequencer COMMAND tst_sequencer)

qt_add_executable(tst_queue tests/tst_queue.cpp)
target_compile_options(tst_queue PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_queue PRIVATE k303_dsp Qt6::Test)
add_test(NAME tst_queue COMMAND tst_queue)

# Plain add_executable: qt_add_executable would run QML import scanning for a
# target linking Qt6::Qml and warn about plugin targets (see NO_IMPORT_SCAN above).
add_executable(tst_pattern
    tests/tst_pattern.cpp
    src/app/PatternModel.h
    src/app/PatternModel.cpp
    src/app/Persistence.h
    src/app/Persistence.cpp
)
target_compile_options(tst_pattern PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_pattern PRIVATE k303_dsp Qt6::Qml Qt6::Test)
add_test(NAME tst_pattern COMMAND tst_pattern)
