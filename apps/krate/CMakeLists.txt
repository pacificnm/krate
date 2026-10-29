# krate: the Krate rack app.

qt_add_executable(krate main.cpp)
qt_add_qml_module(krate
    URI KrateRack
    VERSION 1.0
    NO_IMPORT_SCAN # static plugins are linked and imported explicitly
    IMPORTS Krate
    SOURCES
        RackController.h
        RackController.cpp
        RackMidiLearn.h
        RackMidiLearn.cpp
        RackSession.h
        RackSession.cpp
    QML_FILES
        qml/Main.qml
        qml/TransportBar.qml
        qml/MixerStrip.qml
        qml/Fader.qml
        qml/Meter.qml
)
target_include_directories(krate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(krate PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krate PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(krate PRIVATE
    k303_ui k303_uiplugin
    k909_ui k909_uiplugin
    kalliope_ui kalliope_uiplugin
    klick_ui klick_uiplugin
    krate_qml krate_qmlplugin
    krate_rack krate_host
    Qt6::Gui Qt6::Quick Qt6::QuickControls2
)

# Rack learn and session, with fake instruments (sources compiled directly).
add_executable(tst_racklearn
    tests/tst_racklearn.cpp
    RackController.h
    RackController.cpp
    RackMidiLearn.h
    RackMidiLearn.cpp
    RackSession.h
    RackSession.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.h
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.cpp
)
target_include_directories(tst_racklearn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_racklearn PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_racklearn PRIVATE krate_rack krate_app Qt6::Qml Qt6::Test)
add_test(NAME tst_racklearn COMMAND tst_racklearn)
