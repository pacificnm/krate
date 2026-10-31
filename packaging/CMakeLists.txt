# Installing Krate and packaging it as a .deb (packaging/README.md):
# the eight apps, a menu entry and an icon each, and the docs.
include(GNUInstallDirs)

set(KRATE_APPS krate korrode kessel kalliope krimson kobalt klang konnekt)
install(TARGETS ${KRATE_APPS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
foreach(app IN LISTS KRATE_APPS)
    install(FILES applications/${app}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/${app}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endforeach()
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME copyright)
install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# --- The .deb (cpack -G DEB; packaging/build-deb.sh does it all)
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME krate)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION}.0)
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
# Shared libraries are found by dpkg-shlibdeps; Qt's QML modules are loaded
# at run time, so they're named here. The menu entries start the apps with
# pw-jack (PipeWire's JACK).
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "pipewire-jack, qml6-module-qtqml, qml6-module-qtqml-models, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-templates, qml6-module-qtquick-window, qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
include(CPack)
