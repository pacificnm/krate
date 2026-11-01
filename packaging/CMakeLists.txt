# Installing Krate and packaging it as a .deb (packaging/README.md):
# the eight apps, a menu entry and an icon each, and the docs.
include(GNUInstallDirs)
# Directories as Debian has them (rwxr-xr-x), whatever the builder's umask.
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                                                WORLD_READ WORLD_EXECUTE)

set(KRATE_APPS krate korrode kessel kalliope krimson kobalt klang konnekt)
install(TARGETS ${KRATE_APPS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
foreach(app IN LISTS KRATE_APPS)
    install(FILES applications/${app}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/${app}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endforeach()
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME copyright)
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# A dedicated player (docs/PLAYER.md): Krate full screen under cage, started
# at boot by krate-kiosk@<user>.service once enabled (not by installing).
install(PROGRAMS kiosk/krate-kiosk DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES kiosk/krate-kiosk@.service DESTINATION lib/systemd/system)
install(FILES kiosk/krate-kiosk.pam DESTINATION /etc/pam.d RENAME krate-kiosk)
install(FILES ${PROJECT_SOURCE_DIR}/docs/PLAYER.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# --- The .deb (cpack -G DEB; packaging/build-deb.sh does it all)
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME krate)
# The package's version: the project's, or a release tag's (the release
# workflow passes -DKRATE_PACKAGE_VERSION=<tag without the v>).
set(KRATE_PACKAGE_VERSION "${PROJECT_VERSION}.0" CACHE STRING "Version of the .deb")
set(CPACK_PACKAGE_VERSION ${KRATE_PACKAGE_VERSION})
set(CPACK_PACKAGE_VENDOR "Jaimie Garner")
set(CPACK_PACKAGE_CONTACT "Jaimie Garner <pacificnm@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Synth rack, instruments and modular synth for JACK/PipeWire")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Krate is a suite of instruments and a rack that plays them together.
Each instrument also runs on its own: Korrode (acid bass), Kessel (drum
machine), Kalliope (polysynth), Krimson (multitimbral virtual-analog),
Kobalt (monosynth), Klang (sampler and looper) and Konnekt (a modular
synth with lessons, presets and explanations of every part). Krate itself
adds a mixer, a guitar amp, a song player and a step sequencer.")
set(CPACK_DEBIAN_PACKAGE_SECTION sound)
set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/pacificnm/krate")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)
# Shared libraries are found by dpkg-shlibdeps; what Qt loads at run time
# (the QML modules, the platform plugins, the SQLite driver of the song and
# IR libraries) isn't, so it's named here. The menu entries start the apps with
# pw-jack (PipeWire's JACK).
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "pipewire-jack, qml6-module-qtqml, qml6-module-qtqml-models, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-templates, qml6-module-qtquick-window, qt6-qpa-plugins, libqt6sql6-sqlite")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "cage") # the kiosk (docs/PLAYER.md)
# The PAM file is a configuration file; systemd learns of the kiosk service.
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/kiosk/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/kiosk/postinst;${CMAKE_CURRENT_SOURCE_DIR}/kiosk/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)
