# The Kessel standalone app: a thin main() over the Kessel and Krate QML modules.

qt_add_executable(kessel main.cpp)
target_compile_definitions(kessel PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(kessel PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(kessel PRIVATE
    kessel_ui
    kessel_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
