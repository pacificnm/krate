# The Konnekt app: a thin main() over the Konnekt and Krate QML modules.

qt_add_executable(konnekt main.cpp)
target_compile_definitions(konnekt PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(konnekt PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(konnekt PRIVATE
    konnekt_ui
    konnekt_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
