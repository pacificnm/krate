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
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
)
# Generated type registration includes headers by bare name.
target_include_directories(krimson_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(krimson_ui PUBLIC krimson_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(krimson_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
add_executable(krimson_tst_voice tests/tst_voice.cpp)
target_compile_options(krimson_tst_voice PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(krimson_tst_voice PRIVATE krimson_dsp krate_testing Qt6::Test)
add_test(NAME krimson_tst_voice COMMAND krimson_tst_voice)

add_executable(tst_krimson
    tests/tst_krimson.cpp
    app/KrimsonController.h
    app/KrimsonController.cpp
    app/KrimsonInstrument.h
    app/KrimsonInstrument.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
)
target_include_directories(tst_krimson PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tst_krimson PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_krimson PRIVATE krimson_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_krimson COMMAND tst_krimson)
