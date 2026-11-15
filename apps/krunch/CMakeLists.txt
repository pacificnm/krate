# The Krunch standalone app: the amp in its own window, with what the Krate
# rack gives it around the panel (audio in and out, MIDI Learn, updates, crash
# reports): a main() over the Krunch and Krate QML modules.

qt_add_executable(krunch main.cpp)
set_target_properties(krunch PROPERTIES ENABLE_EXPORTS ON) # -rdynamic: function names in crash reports
target_compile_definitions(krunch PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krunch PRIVATE -Wall -Wextra -Wpedantic)
# Static QML modules: link their plugins (imported in main.cpp).
target_link_libraries(krunch PRIVATE
    krunch_ui
    krunch_uiplugin
    krate_qml
    krate_qmlplugin
    krate_host
    Qt6::Gui
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
)
