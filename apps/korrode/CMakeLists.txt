# The Korrode standalone app: a thin main() over the Korrode and Krate QML modules.

qt_add_executable(korrode main.cpp)
target_compile_definitions(korrode PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(korrode PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(korrode PRIVATE
    korrode_ui
    korrode_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
