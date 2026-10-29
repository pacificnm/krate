# Krunch: guitar amp for the Krate rack (docs/ROADMAP.md).

add_library(krunch_dsp STATIC
    dsp/Filters.h
    dsp/Gate.h
    dsp/Krunch.h
    dsp/Krunch.cpp
    dsp/Oversampler.h
    dsp/Pedal.h
    dsp/PowerAmp.h
    dsp/PowerAmp.cpp
    dsp/Preamp.h
    dsp/Preamp.cpp
    dsp/ToneStack.h
    dsp/ToneStack.cpp
)
target_include_directories(krunch_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(krunch_dsp PUBLIC krate_core)
target_compile_options(krunch_dsp PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(tst_krunch tests/tst_krunch.cpp)
target_include_directories(tst_krunch PRIVATE tools)
target_compile_options(tst_krunch PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_krunch PRIVATE krunch_dsp krate_testing Qt6::Test)
add_test(NAME tst_krunch COMMAND tst_krunch)

# --- krunch_ui: QML module "Krunch": controller and panel
qt_add_library(krunch_ui STATIC)
qt_add_qml_module(krunch_ui
    URI Krunch
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/KrunchController.h
        app/KrunchController.cpp
        app/KrunchInstrument.h
        app/KrunchInstrument.cpp
    QML_FILES
        qml/Panel.qml
)
target_include_directories(krunch_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(krunch_ui PUBLIC krunch_dsp krate_app krate_qml Qt6::Quick)
target_compile_options(krunch_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- krunch-render: offline renders of each channel, and a benchmark
add_executable(krunch-render tools/render_main.cpp tools/SyntheticGuitar.h)
target_include_directories(krunch-render PRIVATE tools)
target_link_libraries(krunch-render PRIVATE krunch_dsp)
target_compile_options(krunch-render PRIVATE -Wall -Wextra -Wpedantic)
