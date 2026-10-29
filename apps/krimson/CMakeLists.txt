# The Krimson standalone app: a thin main() over the Krimson and Krate QML modules.

qt_add_executable(krimson main.cpp)
target_compile_definitions(krimson PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krimson PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(krimson PRIVATE
    krimson_ui
    krimson_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
