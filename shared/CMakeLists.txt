# Instrument-neutral Krate code. Nothing here may include instrument headers.

set(KRATE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- krate_core: interfaces, transport, step clock, MIDI, queues; no Qt, no JACK
add_library(krate_core STATIC
    core/Denormals.h
    core/Instrument.h
    core/MasterStage.h
    core/Midi.h
    core/Midi.cpp
    core/ParamDescriptor.h
    core/ParamValues.h
    core/SpscQueue.h
    core/StandaloneTransport.h
    core/StepClock.h
    core/StepClock.cpp
    tools/WavFile.h
)
target_include_directories(krate_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(krate_core PRIVATE ${KRATE_WARNINGS})

# --- krate_host: standalone JACK host; no Qt
add_library(krate_host STATIC
    host/JackHost.h
    host/JackHost.cpp
)
target_link_libraries(krate_host PUBLIC krate_core PkgConfig::JACK)
target_compile_options(krate_host PRIVATE ${KRATE_WARNINGS})

# --- krate_rack: the rack's realtime core; no Qt, no JACK
add_library(krate_rack STATIC
    rack/Rack.h
    rack/Rack.cpp
)
target_link_libraries(krate_rack PUBLIC krate_core)
target_compile_options(krate_rack PRIVATE ${KRATE_WARNINGS})

# --- krate_app: Qt helpers for any instrument app (files, screenshot hook)
add_library(krate_app STATIC
    app/JsonFile.h
    app/JsonFile.cpp
    app/MidiMapFile.h
    app/MidiMapFile.cpp
    app/PanelFile.h
    app/PanelFile.cpp
    app/ScreenshotHook.h
    app/ScreenshotHook.cpp
)
target_link_libraries(krate_app PUBLIC krate_core Qt6::Core Qt6::Gui Qt6::Quick)
target_compile_options(krate_app PRIVATE -Wall -Wextra -Wpedantic)

# --- krate_qml: QML module "Krate": panel controls, Theme, MidiLearn
qt_add_library(krate_qml STATIC)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(krate_qml
    URI Krate
    VERSION 1.0
    SOURCES
        app/MidiLearnController.h
        app/MidiLearnController.cpp
    QML_FILES
        qml/Knob.qml
        qml/LcdField.qml
        qml/LearnOverlay.qml
        qml/Led.qml
        qml/PanelButton.qml
        qml/PanelLabel.qml
        qml/Theme.qml
)
# Generated type registration includes headers by bare name.
target_include_directories(krate_qml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE app)
target_link_libraries(krate_qml PUBLIC krate_app krate_core Qt6::Qml Qt6::Quick)
target_compile_options(krate_qml PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests for the shared pieces
add_executable(tst_queue tests/tst_queue.cpp)
target_compile_options(tst_queue PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_queue PRIVATE krate_core Qt6::Test)
add_test(NAME tst_queue COMMAND tst_queue)

# Header-only test helpers (TestHost.h) for instrument tests.
add_library(krate_testing INTERFACE)
target_include_directories(krate_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(krate_testing INTERFACE krate_core)

add_executable(tst_rack tests/tst_rack.cpp)
target_compile_options(tst_rack PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_rack PRIVATE krate_rack krate_testing Qt6::Test)
add_test(NAME tst_rack COMMAND tst_rack)
