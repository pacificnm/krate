# K909: drum machine with a tracks x steps sequencer (instruments/k909/README.md).

set(K909_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- k909_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(k909_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Sequencer.h
    dsp/Sequencer.cpp
    dsp/Voices.h
    dsp/Voices.cpp
)
target_include_directories(k909_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(k909_dsp PUBLIC krate_core)
target_compile_options(k909_dsp PRIVATE ${K909_WARNINGS})

# --- k909_ui: QML module "K909": controller and the panel
qt_add_library(k909_ui STATIC)
qt_add_qml_module(k909_ui
    URI K909
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/DrumsController.h
        app/DrumsController.cpp
        app/K909Instrument.h
        app/K909Instrument.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
)
# Generated type registration includes headers by bare name.
target_include_directories(k909_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE app)
target_link_libraries(k909_ui PUBLIC k909_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(k909_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- k909-render: offline renderer and benchmark
add_executable(k909-render tools/render_main.cpp)
target_link_libraries(k909-render PRIVATE k909_dsp)
target_compile_options(k909-render PRIVATE ${K909_WARNINGS})

# --- Tests
foreach(test tst_voices tst_sequencer)
    add_executable(k909_${test} tests/${test}.cpp)
    set_target_properties(k909_${test} PROPERTIES OUTPUT_NAME k909_${test})
    target_compile_options(k909_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(k909_${test} PRIVATE k909_dsp krate_testing Qt6::Test)
    add_test(NAME k909_${test} COMMAND k909_${test})
endforeach()

# Compiles the controller sources directly (plain add_executable) rather than
# linking the QML module library, so no QML plugin is involved.
add_executable(tst_k909
    tests/tst_k909.cpp
    app/DrumsController.h
    app/DrumsController.cpp
    app/K909Instrument.h
    app/K909Instrument.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
)
target_include_directories(tst_k909 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_k909 PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_k909 PRIVATE k909_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_k909 COMMAND tst_k909)
