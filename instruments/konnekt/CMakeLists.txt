# Konnekt: a modular synthesizer, its own app (instruments/konnekt/README.md).

set(KONNEKT_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- konnekt_dsp: the engine and the modules; no Qt, no JACK
add_library(konnekt_dsp STATIC
    dsp/CoreModules.cpp
    dsp/MoreModules.cpp
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Module.h
    dsp/Modules.h
    dsp/PatchSpec.h
    dsp/PatchSpec.cpp
)
target_include_directories(konnekt_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Kalliope's ladder and Kobalt's four-pole filter, as they are.
target_link_libraries(konnekt_dsp PUBLIC krate_core krate_dsp kalliope_dsp kobalt_dsp)
target_compile_options(konnekt_dsp PRIVATE ${KONNEKT_WARNINGS})

# --- konnekt-render: offline renderer and benchmark
add_executable(konnekt-render tools/render_main.cpp)
target_link_libraries(konnekt-render PRIVATE konnekt_dsp)
target_compile_options(konnekt-render PRIVATE ${KONNEKT_WARNINGS})

# --- konnekt_ui: QML module "Konnekt": the controller and the window
qt_add_library(konnekt_ui STATIC)
qt_add_qml_module(konnekt_ui
    URI Konnekt
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/Explainer.h
        app/Explainer.cpp
        app/KonnektController.h
        app/KonnektController.cpp
    QML_FILES
        qml/Main.qml
        qml/ModulePanel.qml
        qml/ExplainPanel.qml
    RESOURCES
        docs/MANUAL.md
        explain/1-general.json
        explain/2-sources.json
        explain/3-shaping.json
        explain/4-effects.json
        lessons/01.json
        lessons/02.json
        lessons/03.json
        lessons/04.json
        lessons/05.json
        lessons/06.json
        lessons/07.json
        lessons/08.json
        lessons/09.json
        lessons/10.json
        presets/01.json
        presets/02.json
        presets/03.json
        presets/04.json
        presets/05.json
        presets/06.json
        presets/07.json
        presets/08.json
        presets/09.json
        presets/10.json
        presets/11.json
        presets/12.json
        presets/13.json
        presets/14.json
        presets/15.json
        presets/16.json
        presets/17.json
)
target_include_directories(konnekt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(konnekt_ui PUBLIC konnekt_dsp krate_host krate_app krate_qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(konnekt_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
foreach(test tst_engine tst_modules)
    add_executable(konnekt_${test} tests/${test}.cpp)
    target_compile_options(konnekt_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(konnekt_${test} PRIVATE konnekt_dsp krate_testing Qt6::Test)
    add_test(NAME konnekt_${test} COMMAND konnekt_${test})
endforeach()

# The app's controller (its sources compiled in: no QML plugin).
add_executable(konnekt_tst_konnekt tests/tst_konnekt.cpp app/KonnektController.h app/KonnektController.cpp
    app/Explainer.h app/Explainer.cpp)
target_include_directories(konnekt_tst_konnekt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(konnekt_tst_konnekt PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(konnekt_tst_konnekt PRIVATE KONNEKT_LESSONS="${CMAKE_CURRENT_SOURCE_DIR}/lessons"
    KONNEKT_PRESETS="${CMAKE_CURRENT_SOURCE_DIR}/presets"
    KONNEKT_EXPLAIN="${CMAKE_CURRENT_SOURCE_DIR}/explain")
target_link_libraries(konnekt_tst_konnekt PRIVATE konnekt_dsp krate_app Qt6::Qml Qt6::Test)
add_test(NAME konnekt_tst_konnekt COMMAND konnekt_tst_konnekt)
