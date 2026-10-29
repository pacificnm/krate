# Krunch: guitar amp for the Krate rack (docs/ROADMAP.md).

add_library(krunch_dsp STATIC
    dsp/Cabinet.h
    dsp/Cabinet.cpp
    dsp/Fft.h
    dsp/Fft.cpp
    dsp/Filters.h
    dsp/Gate.h
    dsp/GreenDrive.h
    dsp/GreenDrive.cpp
    dsp/Krunch.h
    dsp/Krunch.cpp
    dsp/ModPedals.h
    dsp/ModPedals.cpp
    dsp/Oversampler.h
    dsp/Pedal.h
    dsp/Pedal.cpp
    dsp/PowerAmp.h
    dsp/PowerAmp.cpp
    dsp/Preamp.h
    dsp/Preamp.cpp
    dsp/StudioPedals.h
    dsp/StudioPedals.cpp
    dsp/TimePedals.h
    dsp/TimePedals.cpp
    dsp/ToneStack.h
    dsp/ToneStack.cpp
    dsp/Wav.h
    dsp/Wav.cpp
)
target_include_directories(krunch_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(krunch_dsp PUBLIC krate_core krate_dsp)
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
        app/CabinetController.h
        app/CabinetController.cpp
        app/IrLibrary.h
        app/IrLibrary.cpp
        app/KrunchController.h
        app/KrunchController.cpp
        app/KrunchInstrument.h
        app/KrunchInstrument.cpp
        app/PedalController.h
        app/PedalController.cpp
    QML_FILES
        qml/Panel.qml
        qml/IrPicker.qml
        qml/Pedalboard.qml
)
target_include_directories(krunch_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. PRIVATE app)
target_link_libraries(krunch_ui PUBLIC krunch_dsp krate_app krate_qml krate_song Qt6::Quick Qt6::Sql)
target_compile_options(krunch_ui PRIVATE -Wall -Wextra -Wpedantic)

# --- krunch-render: offline renders of each channel, and a benchmark
add_executable(krunch-render tools/render_main.cpp tools/SyntheticGuitar.h)
target_include_directories(krunch-render PRIVATE tools)
target_link_libraries(krunch-render PRIVATE krunch_dsp)
target_compile_options(krunch-render PRIVATE -Wall -Wextra -Wpedantic)

add_executable(tst_cabinet tests/tst_cabinet.cpp)
target_compile_options(tst_cabinet PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_cabinet PRIVATE krunch_dsp krate_testing Qt6::Test)
add_test(NAME tst_cabinet COMMAND tst_cabinet)

# The IR library, with generated WAVs, folders and zips (zlib for their checksums).
add_executable(tst_irlibrary tests/tst_irlibrary.cpp app/IrLibrary.h app/IrLibrary.cpp)
target_include_directories(tst_irlibrary PRIVATE app)
target_compile_options(tst_irlibrary PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_irlibrary PRIVATE krunch_dsp krate_song Qt6::Sql Qt6::Test ZLIB::ZLIB)
add_test(NAME tst_irlibrary COMMAND tst_irlibrary)

add_executable(tst_pedals tests/tst_pedals.cpp)
target_compile_options(tst_pedals PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_pedals PRIVATE krunch_dsp krate_testing Qt6::Test)
add_test(NAME tst_pedals COMMAND tst_pedals)
