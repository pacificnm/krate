# krate: the Krate rack app.

qt_add_executable(krate main.cpp)
qt_add_qml_module(krate
    URI KrateRack
    VERSION 1.0
    NO_IMPORT_SCAN # static plugins are linked and imported explicitly
    IMPORTS Krate
    DEPENDENCIES QtQuick # TabView is a QQuickPaintedItem
    SOURCES
        BackingInstrument.h
        BackingInstrument.cpp
        RackController.h
        RackController.cpp
        RackMidiLearn.h
        RackMidiLearn.cpp
        RackRunControl.h
        RackRunControl.cpp
        RackSession.h
        RackSession.cpp
        NoteTargets.h
        NoteTargets.cpp
        SequencerFile.h
        SequencerFile.cpp
        SequencerController.h
        SequencerController.cpp
        RackSounds.h
        RackSounds.cpp
        SongImport.h
        SongImport.cpp
        SongController.h
        SongController.cpp
        SongLibrary.h
        SongLibrary.cpp
        TabView.h
        TabView.cpp
    QML_FILES
        qml/Main.qml
        qml/TransportBar.qml
        qml/MixerStrip.qml
        qml/Fader.qml
        qml/Meter.qml
        qml/SongScreen.qml
        qml/SeqScreen.qml
        qml/LibraryPanel.qml
)
target_include_directories(krate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(krate PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krate PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(krate PRIVATE
    k303_ui k303_uiplugin
    k909_ui k909_uiplugin
    kalliope_ui kalliope_uiplugin
    krimson_ui krimson_uiplugin
    krunch_ui krunch_uiplugin
    klick_ui klick_uiplugin
    krate_qml krate_qmlplugin
    krate_rack krate_host krate_song krate_seq
    Qt6::Gui Qt6::Sql Qt6::Quick Qt6::QuickControls2
)

# Rack learn and session, with fake instruments (sources compiled directly).
add_executable(tst_racklearn
    tests/tst_racklearn.cpp
    RackController.h
    RackController.cpp
    RackRunControl.h
    RackRunControl.cpp
    RackMidiLearn.h
    RackMidiLearn.cpp
    RackSession.h
    RackSession.cpp
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.h
    ${PROJECT_SOURCE_DIR}/shared/app/MidiLearnController.cpp
)
target_include_directories(tst_racklearn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_racklearn PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_racklearn PRIVATE krate_rack krate_app Qt6::Qml Qt6::Test)
add_test(NAME tst_racklearn COMMAND tst_racklearn)

# The Song screen's controller, with generated song files and fake instruments.
add_executable(tst_song
    tests/tst_song.cpp
    NoteTargets.h
    NoteTargets.cpp
    RackSounds.h
    RackSounds.cpp
    SequencerController.h
    SequencerController.cpp
    SequencerFile.h
    SequencerFile.cpp
    SongImport.h
    SongImport.cpp
    RackController.h
    RackController.cpp
    RackRunControl.h
    RackRunControl.cpp
    SongController.h
    SongController.cpp
    SongLibrary.h
    SongLibrary.cpp
)
target_include_directories(tst_song PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_song PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_song PRIVATE krate_rack krate_song krate_seq krate_app Qt6::Qml Qt6::Sql Qt6::Test)
add_test(NAME tst_song COMMAND tst_song)

# The sequencer's file.
add_executable(tst_sequencerfile tests/tst_sequencerfile.cpp SequencerFile.h SequencerFile.cpp)
target_include_directories(tst_sequencerfile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_sequencerfile PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_sequencerfile PRIVATE krate_seq Qt6::Core Qt6::Test)
add_test(NAME tst_sequencerfile COMMAND tst_sequencerfile)

# Copying a song's bars into the sequencer.
add_executable(tst_songimport tests/tst_songimport.cpp SongImport.h SongImport.cpp)
target_include_directories(tst_songimport PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_songimport PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_songimport PRIVATE krate_seq krate_song Qt6::Test)
add_test(NAME tst_songimport COMMAND tst_songimport)

# The sequencer screen's controller.
add_executable(tst_seqcontroller tests/tst_seqcontroller.cpp SequencerController.h SequencerController.cpp
               SequencerFile.h SequencerFile.cpp NoteTargets.h NoteTargets.cpp SongImport.h SongImport.cpp
               RackRunControl.h RackRunControl.cpp RackSounds.h RackSounds.cpp SongLibrary.h SongLibrary.cpp
               RackController.h RackController.cpp)
target_include_directories(tst_seqcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_seqcontroller PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_seqcontroller PRIVATE krate_seq krate_song krate_rack krate_app Qt6::Sql Qt6::Qml Qt6::Test)
add_test(NAME tst_seqcontroller COMMAND tst_seqcontroller)

# The song library's database, with generated files.
add_executable(tst_library tests/tst_library.cpp SongLibrary.h SongLibrary.cpp)
target_include_directories(tst_library PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_library PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_library PRIVATE Qt6::Sql Qt6::Test)
add_test(NAME tst_library COMMAND tst_library)
