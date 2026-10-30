# k303: acid bass synth with step sequencer (instruments/k303/README.md).

set(K303_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- k303_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(k303_dsp STATIC
    dsp/DiodeLadder.h
    dsp/DiodeLadder.cpp
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Oscillator.h
    dsp/Oscillator.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/ParamStore.h
    dsp/Sequencer.h
    dsp/Sequencer.cpp
    dsp/Voice.h
    dsp/Voice.cpp
)
target_include_directories(k303_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(k303_dsp PUBLIC krate_core krate_dsp)
target_compile_options(k303_dsp PRIVATE ${K303_WARNINGS})

# --- k303_ui: QML module "K303": controllers and the panel
qt_add_library(k303_ui STATIC)
qt_add_qml_module(k303_ui
    URI K303
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/K303Instrument.h
        app/K303Instrument.cpp
        app/PatternFile.h
        app/PatternFile.cpp
        app/PatternModel.h
        app/PatternModel.cpp
        app/SynthController.h
        app/SynthController.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
        qml/PianoKeys.qml
        qml/StepRow.qml
        qml/WaveSwitch.qml
    RESOURCES
        docs/MANUAL.md
)
# Generated type registration includes headers by bare name.
target_include_directories(k303_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE app)
target_link_libraries(k303_ui PUBLIC k303_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(k303_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- k303-render: offline renderer and benchmark
add_executable(k303-render tools/render_main.cpp)
target_link_libraries(k303-render PRIVATE k303_dsp)
target_compile_options(k303-render PRIVATE ${K303_WARNINGS})

# --- Tests
foreach(test tst_params tst_dsp tst_sequencer tst_character)
    add_executable(${test} tests/${test}.cpp)
    target_compile_options(${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${test} PRIVATE k303_dsp krate_testing Qt6::Test)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# These compile the controller sources directly (plain add_executable) rather
# than linking the QML module libraries, so no QML plugin is involved.
add_executable(tst_pattern
    tests/tst_pattern.cpp
    app/PatternFile.h
    app/PatternFile.cpp
    app/PatternModel.h
    app/PatternModel.cpp
)
target_include_directories(tst_pattern PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_pattern PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_pattern PRIVATE k303_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_pattern COMMAND tst_pattern)

add_executable(tst_midi
    tests/tst_midi.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.h
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.h
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.cpp
)
target_compile_options(tst_midi PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_midi PRIVATE k303_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_midi COMMAND tst_midi)

# k303 built through its registry entry, as the standalone app and the rack do.
add_executable(tst_instance
    tests/tst_instance.cpp
    app/K303Instrument.h
    app/K303Instrument.cpp
    app/PatternFile.h
    app/PatternFile.cpp
    app/PatternModel.h
    app/PatternModel.cpp
    app/SynthController.h
    app/SynthController.cpp
)
target_include_directories(tst_instance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_instance PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_instance PRIVATE k303_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_instance COMMAND tst_instance)
