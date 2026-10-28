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
    klick_ui klick_uiplugin
    krate_qml krate_qmlplugin
    krate_rack krate_host
    Qt6::Gui Qt6::Quick Qt6::QuickControls2
)
