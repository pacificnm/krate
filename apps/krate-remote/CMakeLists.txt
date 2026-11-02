# krate-remote: Krate Remote, a player's settings and files from another
# computer, over SSH (docs/REMOTE.md).

qt_add_executable(krate-remote main.cpp)
qt_add_qml_module(krate-remote
    URI KrateRemote
    VERSION 1.0
    NO_IMPORT_SCAN # Qt's own modules are found at run time
    SOURCES
        Shell.h
        Shell.cpp
        RemoteSettings.h
        RemoteSettings.cpp
        RemoteController.h
        RemoteController.cpp
        RemoteFiles.h
        RemoteFiles.cpp
    QML_FILES
        qml/Main.qml
        qml/ConnectBar.qml
        qml/SettingsPage.qml
        qml/FilesPage.qml
        qml/ReportsPage.qml
)
target_include_directories(krate-remote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(krate-remote PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krate-remote PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(krate-remote PRIVATE krate_app Qt6::Gui Qt6::Quick Qt6::QuickControls2)

# Settings, files and transfers against a stand-in ssh that runs the commands
# here, in a scratch "player".
add_executable(tst_remote
    tests/tst_remote.cpp
    Shell.h
    Shell.cpp
    RemoteSettings.h
    RemoteSettings.cpp
    RemoteController.h
    RemoteController.cpp
    RemoteFiles.h
    RemoteFiles.cpp
)
target_include_directories(tst_remote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_remote PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_remote PRIVATE krate_app Qt6::Qml Qt6::Test)
add_test(NAME tst_remote COMMAND tst_remote)
