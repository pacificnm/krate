# The K909 standalone app: a thin main() over the K909 and Krate QML modules.

qt_add_executable(k909 main.cpp)
target_compile_definitions(k909 PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(k909 PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(k909 PRIVATE
    k909_ui
    k909_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
