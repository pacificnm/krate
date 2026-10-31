# Kessel: drum machine with a tracks x steps sequencer (instruments/kessel/README.md).

set(KESSEL_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- kessel_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(kessel_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Sample.h
    dsp/Sample.cpp
    dsp/Sequencer.h
    dsp/Sequencer.cpp
    dsp/Voices.h
    dsp/Voices.cpp
)
# Included as "kessel/dsp/...", "kessel/app/...": Korrode has files of the same names,
# and the rack and krate-render build against both.
target_include_directories(kessel_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(kessel_dsp PUBLIC krate_core krate_dsp)
target_compile_options(kessel_dsp PRIVATE ${KESSEL_WARNINGS})

# --- kessel_ui: QML module "Kessel": controller and the panel
qt_add_library(kessel_ui STATIC)
qt_add_qml_module(kessel_ui
    URI Kessel
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/DrumsController.h
        app/DrumsController.cpp
        app/KitController.h
        app/KitController.cpp
        app/KitFile.h
        app/KitFile.cpp
        app/SampleController.h
        app/SampleController.cpp
        app/KesselInstrument.h
        app/KesselInstrument.cpp
        app/PatternFile.h
        app/PatternFile.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
    RESOURCES
        docs/MANUAL.md
)
# Generated type registration includes headers by bare name.
target_include_directories(kessel_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(kessel_ui PUBLIC kessel_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(kessel_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- kessel-render: offline renderer and benchmark
add_executable(kessel-render tools/render_main.cpp)
target_link_libraries(kessel-render PRIVATE kessel_dsp)
target_compile_options(kessel-render PRIVATE ${KESSEL_WARNINGS})

# --- Tests
foreach(test tst_voices tst_sequencer tst_character)
    add_executable(kessel_${test} tests/${test}.cpp)
    set_target_properties(kessel_${test} PROPERTIES OUTPUT_NAME kessel_${test})
    target_compile_options(kessel_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(kessel_${test} PRIVATE kessel_dsp krate_testing Qt6::Test)
    add_test(NAME kessel_${test} COMMAND kessel_${test})
endforeach()

add_executable(kessel_tst_rack tests/tst_rack.cpp)
target_compile_options(kessel_tst_rack PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kessel_tst_rack PRIVATE kessel_dsp krate_rack krate_testing Qt6::Test)
add_test(NAME kessel_tst_rack COMMAND kessel_tst_rack)

# Compiles the controller sources directly (plain add_executable) rather than
# linking the QML module library, so no QML plugin is involved.
add_executable(tst_kessel
    tests/tst_kessel.cpp
    app/DrumsController.h
    app/DrumsController.cpp
    app/KitController.h
    app/KitController.cpp
    app/KitFile.h
    app/KitFile.cpp
    app/SampleController.h
    app/SampleController.cpp
    app/KesselInstrument.h
    app/KesselInstrument.cpp
    app/PatternFile.h
    app/PatternFile.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
)
target_include_directories(tst_kessel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tst_kessel PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_kessel PRIVATE kessel_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_kessel COMMAND tst_kessel)
