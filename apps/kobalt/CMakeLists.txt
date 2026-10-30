# The Kobalt standalone app: a thin main() over the Kobalt and Krate QML modules.

qt_add_executable(kobalt main.cpp)
target_compile_definitions(kobalt PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(kobalt PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(kobalt PRIVATE
    kobalt_ui
    kobalt_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
