# The Klang standalone app: a thin main() over the Klang and Krate QML modules.

qt_add_executable(klang main.cpp)
target_compile_definitions(klang PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(klang PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(klang PRIVATE
    klang_ui
    klang_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
