# The Kalliope standalone app: a thin main() over the Kalliope and Krate QML modules.

qt_add_executable(kalliope main.cpp)
target_compile_definitions(kalliope PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(kalliope PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(kalliope PRIVATE
    kalliope_ui
    kalliope_uiplugin
    krate_qml
    krate_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)
