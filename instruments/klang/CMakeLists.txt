# Klang: sampler (instruments/klang/README.md).

set(KLANG_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# --- klang_dsp: realtime core (implements krate::IInstrument); no Qt, no JACK
add_library(klang_dsp STATIC
    dsp/Engine.h
    dsp/Engine.cpp
    dsp/Params.h
    dsp/Params.cpp
    dsp/Pattern.h
    dsp/Sample.h
    dsp/StarterKit.h
    dsp/StarterKit.cpp
    dsp/Voice.h
    dsp/Voice.cpp
)
# Included as "klang/dsp/...", "klang/app/...": instruments share file names.
target_include_directories(klang_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(klang_dsp PUBLIC krate_core krate_dsp)
target_compile_options(klang_dsp PRIVATE ${KLANG_WARNINGS})

# --- klang-render: offline renderer and benchmark
add_executable(klang-render tools/render_main.cpp)
target_link_libraries(klang-render PRIVATE klang_dsp)
target_compile_options(klang-render PRIVATE ${KLANG_WARNINGS})

# --- klang_ui: QML module "Klang": controllers and the panel
set(KLANG_APP_SOURCES
    app/KlangController.h
    app/KlangController.cpp
    app/KlangInstrument.h
    app/KlangInstrument.cpp
    app/KlangPadController.h
    app/KlangPadController.cpp
    app/KlangPatternController.h
    app/KlangPatternController.cpp
)
qt_add_library(klang_ui STATIC)
qt_add_qml_module(klang_ui
    URI Klang
    VERSION 1.0
    IMPORTS Krate
    SOURCES ${KLANG_APP_SOURCES}
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
    RESOURCES
        docs/MANUAL.md
)
target_include_directories(klang_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
# krate_song: MP3 decoding (dr_mp3).
target_link_libraries(klang_ui PUBLIC klang_dsp krate_host krate_app krate_qml krate_song Qt6::Quick)
target_compile_options(klang_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests
foreach(test tst_pads tst_patterns)
    add_executable(klang_${test} tests/${test}.cpp)
    target_compile_options(klang_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(klang_${test} PRIVATE klang_dsp krate_testing Qt6::Test)
    add_test(NAME klang_${test} COMMAND klang_${test})
endforeach()

# The controllers, through the instrument's registry entry (sources compiled directly: no QML plugin).
foreach(test tst_klang)
    add_executable(klang_${test}
        tests/${test}.cpp
        ${KLANG_APP_SOURCES}
        ${PROJECT_SOURCE_DIR}/shared/app/ParamController.h
        ${PROJECT_SOURCE_DIR}/shared/app/ParamController.cpp
    )
    target_include_directories(klang_${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_options(klang_${test} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(klang_${test} PRIVATE klang_dsp krate_app krate_song krate_testing Qt6::Qml Qt6::Test)
    add_test(NAME klang_${test} COMMAND klang_${test})
endforeach()
