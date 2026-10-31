# Korrode: acid bass synth with step sequencer (instruments/korrode/README.md).

set(KORRODE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- korrode_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(korrode_dsp STATIC
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
target_include_directories(korrode_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(korrode_dsp PUBLIC krate_core krate_dsp)
target_compile_options(korrode_dsp PRIVATE ${KORRODE_WARNINGS})

# --- korrode_ui: QML module "Korrode": controllers and the panel
qt_add_library(korrode_ui STATIC)
qt_add_qml_module(korrode_ui
    URI Korrode
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/KorrodeInstrument.h
        app/KorrodeInstrument.cpp
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
target_include_directories(korrode_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE app)
target_link_libraries(korrode_ui PUBLIC korrode_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(korrode_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- korrode-render: offline renderer and benchmark
add_executable(korrode-render tools/render_main.cpp)
target_link_libraries(korrode-render PRIVATE korrode_dsp)
target_compile_options(korrode-render PRIVATE ${KORRODE_WARNINGS})

# --- Tests
foreach(test tst_params tst_dsp tst_sequencer tst_character)
    add_executable(${test} tests/${test}.cpp)
    target_compile_options(${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${test} PRIVATE korrode_dsp krate_testing Qt6::Test)
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
target_link_libraries(tst_pattern PRIVATE korrode_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_pattern COMMAND tst_pattern)

add_executable(tst_midi
    tests/tst_midi.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.h
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.h
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.cpp
)
target_compile_options(tst_midi PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_midi PRIVATE korrode_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_midi COMMAND tst_midi)

# Korrode built through its registry entry, as the standalone app and the rack do.
add_executable(tst_instance
    tests/tst_instance.cpp
    app/KorrodeInstrument.h
    app/KorrodeInstrument.cpp
    app/PatternFile.h
    app/PatternFile.cpp
    app/PatternModel.h
    app/PatternModel.cpp
    app/SynthController.h
    app/SynthController.cpp
)
target_include_directories(tst_instance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_instance PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_instance PRIVATE korrode_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_instance COMMAND tst_instance)
