# Krimson: four-part multitimbral virtual-analog synthesizer (instruments/krimson/README.md).

set(KRIMSON_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- krimson_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(krimson_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Filter.h
    dsp/Filter.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Voice.h
    dsp/Voice.cpp
)
# Included as "krimson/dsp/...", "krimson/app/...": instruments share file
# names (Engine.h, Params.h), and the rack builds against all of them.
target_include_directories(krimson_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(krimson_dsp PUBLIC krate_core krate_dsp)
target_compile_options(krimson_dsp PRIVATE ${KRIMSON_WARNINGS})

# --- krimson-render: offline renderer and benchmark
add_executable(krimson-render tools/render_main.cpp)
target_link_libraries(krimson-render PRIVATE krimson_dsp)
target_compile_options(krimson-render PRIVATE ${KRIMSON_WARNINGS})

# --- krimson_ui: QML module "Krimson": controllers and the panel
qt_add_library(krimson_ui STATIC)
qt_add_qml_module(krimson_ui
    URI Krimson
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/KrimsonController.h
        app/KrimsonController.cpp
        app/KrimsonInstrument.h
        app/KrimsonInstrument.cpp
        app/ProgramController.h
        app/ProgramController.cpp
        app/ProgramFile.h
        app/ProgramFile.cpp
        app/SlotController.h
        app/SlotController.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
)
# Generated type registration includes headers by bare name.
target_include_directories(krimson_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(krimson_ui PUBLIC krimson_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(krimson_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
foreach(test tst_voice tst_voices tst_fx tst_arp)
    add_executable(krimson_${test} tests/${test}.cpp)
    target_compile_options(krimson_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(krimson_${test} PRIVATE krimson_dsp krate_testing Qt6::Test)
    add_test(NAME krimson_${test} COMMAND krimson_${test})
endforeach()

foreach(test tst_krimson tst_slots tst_morph tst_programs)
add_executable(${test}
    tests/${test}.cpp
    app/KrimsonController.h
    app/KrimsonController.cpp
    app/KrimsonInstrument.h
    app/KrimsonInstrument.cpp
    app/ProgramController.h
    app/ProgramController.cpp
    app/ProgramFile.h
    app/ProgramFile.cpp
    app/SlotController.h
    app/SlotController.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.h
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.h
    ${PROJECT_SOURCE_DIR}/shared/app/InstrumentMidiLearn.cpp
)
target_include_directories(${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(${test} PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(${test} PRIVATE krimson_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME ${test} COMMAND ${test})
endforeach()
