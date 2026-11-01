# krate: the Krate rack app.

find_package(X11 REQUIRED) # plugin editors' windows (docs/DAW.md, D7)

qt_add_executable(krate main.cpp)
qt_add_qml_module(krate
    URI KrateRack
    VERSION 1.0
    NO_IMPORT_SCAN # static plugins are linked and imported explicitly
    IMPORTS Krate
    DEPENDENCIES QtQuick # TabView is a QQuickPaintedItem
    SOURCES
        SoundIndex.h
        SoundIndex.cpp
        SoundLibrary.h
        SoundLibrary.cpp
        SoundsController.h
        SoundsController.cpp
        InstrumentCopies.h
        InstrumentCopies.cpp
        CrashReportsController.h
        CrashReportsController.cpp
        UpdateController.h
        UpdateController.cpp
        PluginSlots.h
        PluginSlots.cpp
        PluginEditor.h
        PluginEditor.cpp
        ExportController.h
        ExportController.cpp
        ArrangeController.h
        ArrangeController.cpp
        AudioTracks.h
        AudioTracks.cpp
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
        qml/AudioTrackPanel.qml
        qml/ArrangeScreen.qml
        qml/PluginPanel.qml
        qml/SoundsScreen.qml
        qml/CrashReportsPopup.qml
)
target_include_directories(krate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(krate PROPERTIES ENABLE_EXPORTS ON) # -rdynamic: function names in crash reports
target_compile_definitions(krate PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(krate PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(krate PRIVATE
    korrode_ui korrode_uiplugin
    kessel_ui kessel_uiplugin
    kalliope_ui kalliope_uiplugin
    krimson_ui krimson_uiplugin
    kobalt_ui kobalt_uiplugin
    klang_ui klang_uiplugin
    konnekt_ui konnekt_uiplugin
    krunch_ui krunch_uiplugin
    klick_ui klick_uiplugin
    krate_qml krate_qmlplugin
    krate_rack krate_host krate_song krate_seq krate_rec krate_plugin X11::X11
    Qt6::Gui Qt6::Network Qt6::Sql Qt6::Quick Qt6::QuickControls2
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
target_link_libraries(tst_sequencerfile PRIVATE krate_seq krate_rack Qt6::Core Qt6::Test)
add_test(NAME tst_sequencerfile COMMAND tst_sequencerfile)

# Copying a song's bars into the sequencer.
add_executable(tst_songimport tests/tst_songimport.cpp SongImport.h SongImport.cpp)
target_include_directories(tst_songimport PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_songimport PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_songimport PRIVATE krate_seq krate_song Qt6::Test)
add_test(NAME tst_songimport COMMAND tst_songimport)

# The sequencer screen's controller.
add_executable(tst_seqcontroller tests/tst_seqcontroller.cpp SequencerController.h SequencerController.cpp
               ArrangeController.h ArrangeController.cpp AudioTracks.h AudioTracks.cpp
               SequencerFile.h SequencerFile.cpp NoteTargets.h NoteTargets.cpp SongImport.h SongImport.cpp
               RackRunControl.h RackRunControl.cpp RackSounds.h RackSounds.cpp SongLibrary.h SongLibrary.cpp
               RackController.h RackController.cpp)
target_include_directories(tst_seqcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_seqcontroller PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_seqcontroller PRIVATE krate_seq krate_song krate_rack krate_rec krate_app Qt6::Sql Qt6::Qml Qt6::Test)
add_test(NAME tst_seqcontroller COMMAND tst_seqcontroller)

# The song library's database, with generated files.
add_executable(tst_library tests/tst_library.cpp SongLibrary.h SongLibrary.cpp)
target_include_directories(tst_library PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_library PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_library PRIVATE Qt6::Sql Qt6::Test)
add_test(NAME tst_library COMMAND tst_library)

# The audio tracks' controller: takes recorded, kept, reloaded, deleted.
add_executable(tst_audiotracks tests/tst_audiotracks.cpp AudioTracks.h AudioTracks.cpp
               RackController.h RackController.cpp RackRunControl.h RackRunControl.cpp)
target_include_directories(tst_audiotracks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_audiotracks PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_audiotracks PRIVATE krate_rec krate_rack krate_app Qt6::Qml Qt6::Test)
add_test(NAME tst_audiotracks COMMAND tst_audiotracks)

# Copies of an instrument (docs/DAW.md#copies): entries, the stand-in device,
# building one with the first one's files, the rack controller, the targets.
add_executable(tst_copies tests/tst_copies.cpp InstrumentCopies.h InstrumentCopies.cpp NoteTargets.h NoteTargets.cpp
               RackController.h RackController.cpp RackRunControl.h RackRunControl.cpp)
target_include_directories(tst_copies PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_copies PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_copies PRIVATE krate_rack krate_app Qt6::Qml Qt6::Test)
add_test(NAME tst_copies COMMAND tst_copies)

# Updates (docs/UPDATES.md): GitHub's answer, versions, the updater's status.
add_executable(tst_updates tests/tst_updates.cpp UpdateController.h UpdateController.cpp)
target_include_directories(tst_updates PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_updates PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_updates PRIVATE Qt6::Network Qt6::Qml Qt6::Test)
add_test(NAME tst_updates COMMAND tst_updates)

# The sound library: categories, WAV headers, folders indexed in place.
add_executable(tst_sounds tests/tst_sounds.cpp SoundIndex.h SoundIndex.cpp SoundLibrary.h SoundLibrary.cpp)
target_include_directories(tst_sounds PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tst_sounds PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tst_sounds PRIVATE krate_rec Qt6::Sql Qt6::Test)
add_test(NAME tst_sounds COMMAND tst_sounds)
