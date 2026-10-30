# Kobalt: monophonic synthesizer (instruments/kobalt/README.md).

set(KOBALT_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- kobalt_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(kobalt_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Filter.h
    dsp/Filter.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Sequence.h
    dsp/Sequence.cpp
    dsp/Voice.h
    dsp/Voice.cpp
)
# Included as "kobalt/dsp/...", "kobalt/app/...": instruments share file
# names (Engine.h, Params.h), and the rack builds against all of them.
target_include_directories(kobalt_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(kobalt_dsp PUBLIC krate_core krate_dsp)
target_compile_options(kobalt_dsp PRIVATE ${KOBALT_WARNINGS})

# --- kobalt-render: offline renderer and benchmark
add_executable(kobalt-render tools/render_main.cpp)
target_link_libraries(kobalt-render PRIVATE kobalt_dsp)
target_compile_options(kobalt-render PRIVATE ${KOBALT_WARNINGS})

# --- kobalt_ui: QML module "Kobalt": controllers and the panel
set(KOBALT_APP_SOURCES
    app/KobaltController.h
    app/KobaltController.cpp
    app/KobaltInstrument.h
    app/KobaltInstrument.cpp
    app/KobaltPatchController.h
    app/KobaltPatchController.cpp
    app/PatchFile.h
    app/PatchFile.cpp
    app/KobaltSequenceController.h
    app/KobaltSequenceController.cpp
    app/SequenceFile.h
    app/SequenceFile.cpp
)
qt_add_library(kobalt_ui STATIC)
qt_add_qml_module(kobalt_ui
    URI Kobalt
    VERSION 1.0
    IMPORTS Krate
    SOURCES ${KOBALT_APP_SOURCES}
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
    RESOURCES
        docs/MANUAL.md
)
# Generated type registration includes headers by bare name.
target_include_directories(kobalt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(kobalt_ui PUBLIC kobalt_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(kobalt_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
foreach(test tst_voice tst_arp)
    add_executable(kobalt_${test} tests/${test}.cpp)
    target_compile_options(kobalt_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(kobalt_${test} PRIVATE kobalt_dsp krate_testing Qt6::Test)
    add_test(NAME kobalt_${test} COMMAND kobalt_${test})
endforeach()

# The controllers, through the instrument's registry entry: compiled directly
# (plain add_executable) rather than linking the QML module, so no QML plugin is involved.
foreach(test tst_kobalt tst_patches tst_sequencer)
    add_executable(kobalt_${test}
        tests/${test}.cpp
        ${KOBALT_APP_SOURCES}
        ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
        ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
    )
    target_include_directories(kobalt_${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_options(kobalt_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(kobalt_${test} PRIVATE kobalt_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
    add_test(NAME kobalt_${test} COMMAND kobalt_${test})
endforeach()
