# The k303 standalone app: a thin main() over the k303 and Krate QML modules.

qt_add_executable(k303 main.cpp)
target_compile_definitions(k303 PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(k303 PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(k303 PRIVATE
    k303_ui
    k303_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
