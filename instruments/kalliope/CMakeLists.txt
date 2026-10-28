# Kalliope: eight-voice polyphonic synthesizer (instruments/kalliope/README.md).

set(KALLIOPE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- kalliope_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(kalliope_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Params.h
    dsp/Params.cpp
)
# Included as "kalliope/dsp/...", "kalliope/app/...": instruments share file
# names (Engine.h, Params.h), and the rack builds against all of them.
target_include_directories(kalliope_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(kalliope_dsp PUBLIC krate_core krate_dsp)
target_compile_options(kalliope_dsp PRIVATE ${KALLIOPE_WARNINGS})

# --- kalliope_ui: QML module "Kalliope": controller and the panel
qt_add_library(kalliope_ui STATIC)
qt_add_qml_module(kalliope_ui
    URI Kalliope
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/KalliopeController.h
        app/KalliopeController.cpp
        app/KalliopeInstrument.h
        app/KalliopeInstrument.cpp
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
)
# Generated type registration includes headers by bare name.
target_include_directories(kalliope_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(kalliope_ui PUBLIC kalliope_dsp krate_host krate_app krate_qml Qt6::Quick)
target_compile_options(kalliope_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
# Compiles the controller sources directly (plain add_executable) rather than
# linking the QML module library, so no QML plugin is involved.
add_executable(tst_kalliope
    tests/tst_kalliope.cpp
    app/KalliopeController.h
    app/KalliopeController.cpp
    app/KalliopeInstrument.h
    app/KalliopeInstrument.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
    ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
)
target_include_directories(tst_kalliope PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tst_kalliope PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_kalliope PRIVATE kalliope_dsp krate_app krate_testing Qt6::Qml Qt6::Test)
add_test(NAME tst_kalliope COMMAND tst_kalliope)
