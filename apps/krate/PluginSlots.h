#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "app/InstrumentRegistry.h"
#include "plugin/Clap.h"

class QQmlEngine;
class QJSEngine;
class PluginEditor;

// QML singleton "Plugins": CLAP plugins in the rack (docs/DAW.md, D7). Eight
// slots, "Plugin 1".."Plugin 8", each a rack device added from + like an
// instrument; a slot holds a plugin you pick from those found on the system.
// An instrument plugin plays like the built-in instruments (the keyboard, the
// sequencer, the arranger, the song player, its mixer strip, automation); an
// effect takes another device's output as its input (SOURCE). Its editor
// opens in a window of its own; its knobs are on the slot's panel too. Which
// plugin each slot holds, and its state, are kept in plugins.json.
//
// The plugins' main-thread needs (timers, file descriptors, call-backs,
// restarts) are met here, on the GUI thread.
class PluginSlotsController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Plugins)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed) // a plugin loaded, its knobs moved
    Q_PROPERTY(QVariantList available READ available NOTIFY availableChanged)

public:
    static constexpr int kSlots = 8;

    // `file`: plugins.json; `searchPaths`: where plugins are looked for.
    PluginSlotsController(QString file, std::vector<std::string> searchPaths, QObject *parent = nullptr);
    ~PluginSlotsController() override;
    static PluginSlotsController *create(QQmlEngine *, QJSEngine *);

    krate::clap::PluginSlot &slot(int i) { return m_slots[std::size_t(i)].engine; }
    void setDevice(int slot, int rackDevice) { m_slots[std::size_t(slot)].device = rackDevice; }
    // The rate the rack runs at when nothing has prepared a slot yet (the host's; 0: unknown).
    void setHostRate(std::function<double()> rate) { m_hostRate = std::move(rate); }
    // Plugins as they were (after the rack exists).
    void restore();
    void save();
    // Before the rack and the host go: every plugin deactivated, the state kept.
    void finish();

    int revision() const { return m_revision; }
    QVariantList available() const; // [{path, id, name, vendor, kind: "Instrument"|"Effect"|"Plugin"}]
    Q_INVOKABLE void rescan();

    Q_INVOKABLE int slotOf(int rackDevice) const; // -1: not a plugin slot
    Q_INVOKABLE bool load(int slot, const QString &path, const QString &id);
    Q_INVOKABLE void unload(int slot);
    Q_INVOKABLE QString pluginName(int slot) const; // "" when empty
    Q_INVOKABLE QString pluginVendor(int slot) const;
    Q_INVOKABLE bool isEffect(int slot) const;
    Q_INVOKABLE QString message(int slot) const; // why the last load failed, say
    // Its editor: in a window of its own.
    Q_INVOKABLE bool hasEditor(int slot) const;
    Q_INVOKABLE bool editorOpen(int slot) const;
    Q_INVOKABLE void toggleEditor(int slot);
    // Its knobs.
    Q_INVOKABLE int paramCount(int slot) const;
    Q_INVOKABLE QString paramLabel(int slot, int index) const;
    Q_INVOKABLE qreal param(int slot, int index) const;
    Q_INVOKABLE void setParam(int slot, int index, qreal value);

signals:
    void changed();
    void availableChanged();
    void pluginsChanged(); // a slot's plugin came or went (its knobs with it)

private:
    struct Slot;
    // What a plugin asks of the main thread, for one slot.
    class Services final : public krate::clap::HostServices
    {
    public:
        Services(PluginSlotsController &owner, int slot) : m_owner(owner), m_slot(slot) {}
        void requestCallback() override;
        void requestRestart() override;
        bool registerTimer(std::uint32_t periodMs, clap_id *id) override;
        bool unregisterTimer(clap_id id) override;
        bool registerFd(int fd, clap_posix_fd_flags_t flags) override;
        bool modifyFd(int fd, clap_posix_fd_flags_t flags) override;
        bool unregisterFd(int fd) override;
        bool guiRequestResize(std::uint32_t width, std::uint32_t height) override;
        bool guiRequestShow() override;
        bool guiRequestHide() override;
        void guiClosed(bool destroyed) override;
        void stateDirty() override;

    private:
        PluginSlotsController &m_owner;
        int m_slot;
    };
    struct Fd
    {
        std::unique_ptr<QSocketNotifier> read, write, error;
    };
    struct Retired
    {
        std::unique_ptr<krate::clap::Instance> instance;
        int slot = 0;
        std::uint64_t block = 0;
        QElapsedTimer since;
    };
    struct Slot
    {
        krate::clap::PluginSlot engine;
        std::unique_ptr<Services> services;
        std::unique_ptr<krate::clap::Instance> instance;
        std::shared_ptr<krate::clap::Library> library;
        QString path, id, message;
        int device = -1;
        std::map<clap_id, std::unique_ptr<QTimer>> timers;
        std::map<int, Fd> fds;
        std::unique_ptr<PluginEditor> editor;
        std::atomic<bool> callback{false}, restart{false};
    };

    Slot &at(int i) { return m_slots[std::size_t(i)]; }
    const Slot &at(int i) const { return m_slots[std::size_t(i)]; }
    void poll();           // call-backs, restarts, retired plugins, knobs moved
    void dropInstance(int slot); // out of the slot, freed once the audio thread has moved on
    double rateFor(int slot) const;
    void bump();

    static inline PluginSlotsController *s_instance = nullptr;
    QString m_file;
    std::vector<std::string> m_searchPaths;
    std::function<double()> m_hostRate = [] { return 0.0; };
    mutable std::vector<krate::clap::PluginInfo> m_available;
    mutable bool m_scanned = false;
    std::array<Slot, kSlots> m_slots;
    std::vector<Retired> m_retired;
    int m_revision = 0;
    bool m_dirty = false;
    QTimer m_pollTimer, m_saveTimer;
    clap_id m_nextTimer = 1;
};

// The slots' registry entries: "plugin1".."plugin8", each a rack device with
// its panel (PluginPanel), in the rack once added from +.
QList<krate::InstrumentEntry> pluginSlotEntries(PluginSlotsController &owner);
