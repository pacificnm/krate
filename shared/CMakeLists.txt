# Instrument-neutral Krate code. Nothing here may include instrument headers.

set(KRATE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- krate_core: interfaces, transport, step clock, MIDI, queues; no Qt, no JACK
add_library(krate_core STATIC
    core/Denormals.h
    core/EventSource.h
    core/Instrument.h
    core/MasterStage.h
    core/Midi.h
    core/Midi.cpp
    core/ParamDescriptor.h
    core/ParamValues.h
    core/SpscQueue.h
    core/Transport.h
    core/StepClock.h
    core/StepClock.cpp
    tools/WavFile.h
)
target_include_directories(krate_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(krate_core PRIVATE ${KRATE_WARNINGS})

# --- krate_dsp: DSP building blocks shared by instruments (oversampling
# decimator, PolyBLEP, band-limited oscillator, ADSR, LFO, chorus, delay, reverb); no Qt, no JACK.
# Named so they never collide with an instrument's own dsp/ files (k303 has dsp/Oscillator.h).
add_library(krate_dsp STATIC
    dsp/BlepOscillator.h
    dsp/BlepOscillator.cpp
    dsp/Effects.h
    dsp/Effects.cpp
    dsp/Envelope.h
    dsp/HalfbandDecimator.h
    dsp/Lfo.h
    dsp/PolyBlep.h
    dsp/Reverb.h
    dsp/Reverb.cpp
)
target_include_directories(krate_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(krate_dsp PRIVATE ${KRATE_WARNINGS})

# --- krate_host: standalone JACK host; no Qt
add_library(krate_host STATIC
    host/AudioHost.h
    host/JackProcessor.h
    host/JackHost.h
    host/JackHost.cpp
)
target_link_libraries(krate_host PUBLIC krate_core PkgConfig::JACK)
target_compile_options(krate_host PRIVATE ${KRATE_WARNINGS})

# --- krate_rack: the rack's realtime core; no Qt, no JACK
add_library(krate_rack STATIC
    rack/Rack.h
    rack/Rack.cpp
    rack/RackProcessor.h
)
target_link_libraries(krate_rack PUBLIC krate_core)
target_compile_options(krate_rack PRIVATE ${KRATE_WARNINGS})

# --- krate_song: songs to play along with (tempo map, MIDI files, .gp tab files, the player); no Qt
find_package(ZLIB REQUIRED)
add_library(krate_song STATIC
    song/Song.h
    song/Song.cpp
    song/MidiFile.h
    song/MidiFile.cpp
    song/Score.h
    song/GpFile.h
    song/GpFile.cpp
    song/Xml.h
    song/Xml.cpp
    song/Zip.h
    song/Zip.cpp
    song/Arrange.h
    song/Arrange.cpp
    song/SongPlayer.h
    song/SongPlayer.cpp
    song/TabLayout.h
    song/TabLayout.cpp
)
target_include_directories(krate_song PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(krate_song PUBLIC krate_core PRIVATE ZLIB::ZLIB)
target_compile_options(krate_song PRIVATE ${KRATE_WARNINGS})

# --- krate_app: Qt helpers for any instrument app (files, screenshot hook)
add_library(krate_app STATIC
    app/InstrumentRegistry.h
    app/InstrumentRegistry.cpp
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
        app/HostController.h
        app/HostController.cpp
        app/InstrumentRegistry.h
        app/InstrumentMidiLearn.h
        app/InstrumentMidiLearn.cpp
        app/MidiLearnController.h
        app/MidiLearnController.cpp
        app/ParamController.h
        app/ParamController.cpp
    QML_FILES
        qml/Knob.qml
        qml/LcdField.qml
        qml/LearnOverlay.qml
        qml/Led.qml
        qml/PanelButton.qml
        qml/PanelLabel.qml
        qml/PanelSlider.qml
        qml/PanelSwitch.qml
        qml/PianoKeyboard.qml
        qml/RingKnob.qml
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

# QML tests (shared controls), when Qt's qmltestrunner is installed.
find_program(QMLTESTRUNNER qmltestrunner HINTS ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} /usr/lib/qt6/bin)
if(QMLTESTRUNNER)
    add_test(NAME tst_qml_shared COMMAND ${QMLTESTRUNNER} -platform offscreen -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml)
endif()

add_executable(tst_midifile tests/tst_midifile.cpp)
target_compile_options(tst_midifile PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_midifile PRIVATE krate_song Qt6::Test)
add_test(NAME tst_midifile COMMAND tst_midifile)

add_executable(tst_gpfile tests/tst_gpfile.cpp)
target_compile_options(tst_gpfile PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_gpfile PRIVATE krate_song ZLIB::ZLIB Qt6::Test)
add_test(NAME tst_gpfile COMMAND tst_gpfile)

add_executable(tst_songplayer tests/tst_songplayer.cpp)
target_compile_options(tst_songplayer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_songplayer PRIVATE krate_song krate_rack Qt6::Test)
add_test(NAME tst_songplayer COMMAND tst_songplayer)

add_executable(tst_tablayout tests/tst_tablayout.cpp)
target_compile_options(tst_tablayout PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_tablayout PRIVATE krate_song Qt6::Test)
add_test(NAME tst_tablayout COMMAND tst_tablayout)
