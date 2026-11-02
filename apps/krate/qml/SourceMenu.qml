pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// Where a device's audio comes from, by name: the interface's jacks (IN 1,
// IN 2... and what each is wired to, set in AUDIO), then the instruments
// that can feed it.
Menu {
    id: menu

    property int device: -1

    Instantiator {
        model: menu.visible ? Rack.inputSources(menu.device) : []
        delegate: MenuItem {
            required property var modelData
            text: modelData.label
            checkable: true
            checked: Rack.inputSource(menu.device) === modelData.source
            onTriggered: Rack.setInputSource(menu.device, modelData.source)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
