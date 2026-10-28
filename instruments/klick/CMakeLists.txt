# Klick: metronome instrument for the Krate rack.

add_library(klick_dsp STATIC
    dsp/Klick.h
    dsp/Klick.cpp
)
target_include_directories(klick_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(klick_dsp PUBLIC krate_core)
target_compile_options(klick_dsp PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(tst_klick tests/tst_klick.cpp)
target_compile_options(tst_klick PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_klick PRIVATE klick_dsp krate_testing Qt6::Test)
add_test(NAME tst_klick COMMAND tst_klick)

# --- klick_ui: QML module "Klick": controller and panel
qt_add_library(klick_ui STATIC)
qt_add_qml_module(klick_ui
    URI Klick
    VERSION 1.0
    IMPORTS Krate
    SOURCES
        app/KlickController.h
        app/KlickController.cpp
        app/KlickInstrument.h
        app/KlickInstrument.cpp
    QML_FILES
        qml/Panel.qml
)
target_include_directories(klick_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE app)
target_link_libraries(klick_ui PUBLIC klick_dsp krate_app krate_qml Qt6::Quick)
target_compile_options(klick_ui PRIVATE -Wall -Wextra -Wpedantic)
