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
        presets/18.json
        presets/19.json
        presets/20.json
        presets/21.json
        presets/22.json
        presets/23.json
        presets/24.json
        presets/25.json
        presets/26.json
        presets/27.json
        presets/28.json
        presets/29.json
        presets/30.json
        presets/31.json
        presets/32.json
        presets/33.json
        presets/34.json
        presets/35.json
        presets/36.json
        presets/37.json
        presets/38.json
        presets/39.json
        presets/40.json
        presets/41.json
        presets/42.json
        presets/43.json
        presets/44.json
        presets/45.json
        presets/46.json
        presets/47.json
        presets/48.json
        presets/49.json
        presets/50.json
        presets/51.json
        presets/52.json
        presets/53.json
        presets/54.json
        presets/55.json
        presets/56.json
        presets/57.json
        presets/58.json
        presets/59.json
        presets/60.json
        presets/61.json
        presets/62.json
        presets/63.json
        presets/64.json
        presets/65.json
        presets/66.json
        presets/67.json
        presets/68.json
        presets/69.json
        presets/70.json
        presets/71.json
        presets/72.json
        presets/73.json
        presets/74.json
        presets/75.json
        presets/76.json
        presets/77.json
        presets/78.json
        presets/79.json
        presets/80.json
        presets/81.json
        presets/82.json
        presets/83.json
        presets/84.json
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
