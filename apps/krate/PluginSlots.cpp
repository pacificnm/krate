#include "PluginSlots.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "PluginEditor.h"

namespace {
constexpr std::uint32_t kMaxFrames = 4096; // the rack's largest block

// A rack device that is one of the slots.
class PluginSlotInstance final : public krate::InstrumentInstance
{
public:
    explicit PluginSlotInstance(krate::clap::PluginSlot &slot) : m_slot(slot) {}
    krate::IInstrument &instrument() override { return m_slot; }
    // Its plugin's state is kept in plugins.json, not a song's sounds.
    QJsonObject saveSound() override { return {}; }
    void loadSound(const QJsonObject &) override {}

private:
    krate::clap::PluginSlot &m_slot;
};
} // namespace

// --- What the plugins ask of the main thread ------------------------------------------

void PluginSlotsController::Services::requestCallback()
{
    m_owner.at(m_slot).callback.store(true); // any thread: poll() calls it back
}

void PluginSlotsController::Services::requestRestart()
{
    m_owner.at(m_slot).restart.store(true);
}

bool PluginSlotsController::Services::registerTimer(std::uint32_t periodMs, clap_id *id)
{
    Slot &s = m_owner.at(m_slot);
    const clap_id tid = m_owner.m_nextTimer++;
    auto timer = std::make_unique<QTimer>();
    timer->setInterval(int(std::max<std::uint32_t>(periodMs, 1)));
    const int slot = m_slot;
    QObject::connect(timer.get(), &QTimer::timeout, &m_owner, [this, slot, tid] {
        if (auto *instance = m_owner.at(slot).instance.get())
            instance->onTimer(tid);
    });
    timer->start();
    s.timers[tid] = std::move(timer);
    *id = tid;
    return true;
}

bool PluginSlotsController::Services::unregisterTimer(clap_id id)
{
    return m_owner.at(m_slot).timers.erase(id) > 0;
}

bool PluginSlotsController::Services::registerFd(int fd, clap_posix_fd_flags_t flags)
{
    Slot &s = m_owner.at(m_slot);
    if (s.fds.count(fd))
        return false;
    s.fds[fd] = Fd{};
    return modifyFd(fd, flags);
}

bool PluginSlotsController::Services::modifyFd(int fd, clap_posix_fd_flags_t flags)
{
    Slot &s = m_owner.at(m_slot);
    auto it = s.fds.find(fd);
    if (it == s.fds.end())
        return false;
    const int slot = m_slot;
    const auto watch = [&](std::unique_ptr<QSocketNotifier> &n, bool want, QSocketNotifier::Type type,
                           clap_posix_fd_flags_t flag) {
        if (!want) {
            n.reset();
            return;
        }
        if (n)
            return;
        n = std::make_unique<QSocketNotifier>(fd, type);
        QObject::connect(n.get(), &QSocketNotifier::activated, &m_owner, [this, slot, fd, flag] {
            if (auto *instance = m_owner.at(slot).instance.get())
                instance->onFd(fd, flag);
        });
    };
    watch(it->second.read, flags & CLAP_POSIX_FD_READ, QSocketNotifier::Read, CLAP_POSIX_FD_READ);
    watch(it->second.write, flags & CLAP_POSIX_FD_WRITE, QSocketNotifier::Write, CLAP_POSIX_FD_WRITE);
    watch(it->second.error, flags & CLAP_POSIX_FD_ERROR, QSocketNotifier::Exception, CLAP_POSIX_FD_ERROR);
    return true;
}

bool PluginSlotsController::Services::unregisterFd(int fd)
{
    return m_owner.at(m_slot).fds.erase(fd) > 0;
}

bool PluginSlotsController::Services::guiRequestResize(std::uint32_t width, std::uint32_t height)
{
    const Slot &s = m_owner.at(m_slot);
    return s.editor && s.editor->resize(width, height);
}

bool PluginSlotsController::Services::guiRequestShow()
{
    const Slot &s = m_owner.at(m_slot);
    if (s.editor)
        s.editor->raise();
    return bool(s.editor);
}

bool PluginSlotsController::Services::guiRequestHide()
{
    return false;
}

void PluginSlotsController::Services::guiClosed(bool)
{
    Slot &s = m_owner.at(m_slot);
    if (s.editor) // the plugin closed it: our window goes too (later: not from inside its call)
        QMetaObject::invokeMethod(&m_owner, [this] {
            m_owner.at(m_slot).editor.reset();
            m_owner.bump();
        }, Qt::QueuedConnection);
}

void PluginSlotsController::Services::stateDirty()
{
    m_owner.m_dirty = true;
    m_owner.m_saveTimer.start();
}

// --- The slots ------------------------------------------------------------------------

PluginSlotsController::PluginSlotsController(QString file, std::vector<std::string> searchPaths, QObject *parent)
    : QObject(parent)
    , m_file(std::move(file))
    , m_searchPaths(std::move(searchPaths))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    for (int i = 0; i < kSlots; ++i)
        at(i).services = std::make_unique<Services>(*this, i);
    m_pollTimer.setInterval(30);
    connect(&m_pollTimer, &QTimer::timeout, this, &PluginSlotsController::poll);
    m_pollTimer.start();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &PluginSlotsController::save);
}

PluginSlotsController::~PluginSlotsController()
{
    finish();
    s_instance = nullptr;
}

PluginSlotsController *PluginSlotsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void PluginSlotsController::finish()
{
    if (!m_pollTimer.isActive())
        return; // done already
    save();
    m_pollTimer.stop();
    for (int i = 0; i < kSlots; ++i) {
        Slot &s = at(i);
        s.editor.reset();
        s.engine.setInstance(nullptr);
        s.timers.clear();
        s.fds.clear();
        s.instance.reset(); // deactivated (no audio thread by now, or it lets go within its wait)
    }
    m_retired.clear();
}

QVariantList PluginSlotsController::available() const
{
    if (!m_scanned) {
        m_available = krate::clap::scan(m_searchPaths);
        m_scanned = true;
    }
    QVariantList list;
    for (const auto &p : m_available)
        list.append(QVariantMap{
            {QStringLiteral("path"), QString::fromStdString(p.path)},
            {QStringLiteral("id"), QString::fromStdString(p.id)},
            {QStringLiteral("name"), QString::fromStdString(p.name)},
            {QStringLiteral("vendor"), QString::fromStdString(p.vendor)},
            {QStringLiteral("kind"), p.instrument ? QStringLiteral("Instrument") : p.effect ? QStringLiteral("Effect")
                                                                                            : QStringLiteral("Plugin")},
        });
    return list;
}

void PluginSlotsController::rescan()
{
    m_scanned = false;
    emit availableChanged();
}

int PluginSlotsController::slotOf(int rackDevice) const
{
    for (int i = 0; i < kSlots; ++i)
        if (rackDevice >= 0 && at(i).device == rackDevice)
            return i;
    return -1;
}

double PluginSlotsController::rateFor(int slot) const
{
    if (const double r = at(slot).engine.sampleRate(); r > 0.0)
        return r;
    if (const double r = m_hostRate(); r > 0.0)
        return r;
    return 48000.0;
}

bool PluginSlotsController::load(int slot, const QString &path, const QString &id)
{
    if (slot < 0 || slot >= kSlots)
        return false;
    Slot &s = at(slot);
    std::string error;
    auto library = s.library && QString::fromStdString(s.library->path()) == path
                       ? s.library
                       : krate::clap::Library::open(path.toStdString(), &error);
    std::unique_ptr<krate::clap::Instance> instance;
    if (library)
        instance = krate::clap::Instance::create(library, id.toStdString(), s.services.get(), &error);
    if (instance && !instance->activate(rateFor(slot), kMaxFrames)) {
        error = "it wouldn't start at this sample rate";
        instance.reset();
    }
    if (!instance) {
        s.message = QStringLiteral("Couldn't load: %1").arg(QString::fromStdString(error));
        bump();
        return false;
    }
    dropInstance(slot);
    s.library = library;
    s.instance = std::move(instance);
    s.path = path;
    s.id = id;
    s.message.clear();
    s.engine.setInstance(s.instance.get());
    m_dirty = true;
    save();
    bump();
    emit pluginsChanged();
    return true;
}

void PluginSlotsController::unload(int slot)
{
    if (slot < 0 || slot >= kSlots || !at(slot).instance)
        return;
    dropInstance(slot);
    at(slot).path.clear();
    at(slot).id.clear();
    save();
    bump();
    emit pluginsChanged();
}

void PluginSlotsController::dropInstance(int slot)
{
    Slot &s = at(slot);
    if (!s.instance)
        return;
    s.editor.reset();
    s.timers.clear();
    s.fds.clear();
    // Stopped by the audio thread while still in the slot (or by us, if none runs), then out.
    s.instance->deactivate();
    s.engine.setInstance(nullptr);
    Retired r{std::move(s.instance), slot, s.engine.blocks(), {}};
    r.since.start();
    m_retired.push_back(std::move(r));
}

QString PluginSlotsController::pluginName(int slot) const
{
    return slot >= 0 && slot < kSlots && at(slot).instance ? QString::fromStdString(at(slot).instance->info().name) : QString();
}

QString PluginSlotsController::pluginVendor(int slot) const
{
    return slot >= 0 && slot < kSlots && at(slot).instance ? QString::fromStdString(at(slot).instance->info().vendor) : QString();
}

bool PluginSlotsController::isEffect(int slot) const
{
    return slot >= 0 && slot < kSlots && at(slot).instance && at(slot).instance->hasAudioInput();
}

QString PluginSlotsController::message(int slot) const
{
    return slot >= 0 && slot < kSlots ? at(slot).message : QString();
}

bool PluginSlotsController::hasEditor(int slot) const
{
    return slot >= 0 && slot < kSlots && at(slot).instance && at(slot).instance->gui();
}

bool PluginSlotsController::editorOpen(int slot) const
{
    return slot >= 0 && slot < kSlots && at(slot).editor && at(slot).editor->isOpen();
}

void PluginSlotsController::toggleEditor(int slot)
{
    if (!hasEditor(slot))
        return;
    Slot &s = at(slot);
    if (s.editor) {
        s.editor.reset();
    } else {
        s.editor = std::make_unique<PluginEditor>(*s.instance, QStringLiteral("%1 (Plugin %2)").arg(pluginName(slot)).arg(slot + 1));
        s.editor->onClosed = [this, slot] {
            QMetaObject::invokeMethod(this, [this, slot] { // not from inside the editor's own event handling
                at(slot).editor.reset();
                bump();
            }, Qt::QueuedConnection);
        };
        QString error;
        if (!s.editor->open(&error)) {
            s.editor.reset();
            s.message = error;
        }
    }
    bump();
}

int PluginSlotsController::paramCount(int slot) const
{
    return slot >= 0 && slot < kSlots ? int(at(slot).engine.parameters().size()) : 0;
}

QString PluginSlotsController::paramLabel(int slot, int index) const
{
    const auto params = slot >= 0 && slot < kSlots ? at(slot).engine.parameters() : std::span<const krate::ParamDescriptor>{};
    if (index < 0 || index >= int(params.size()))
        return {};
    return QString::fromUtf8(params[std::size_t(index)].label.data(), qsizetype(params[std::size_t(index)].label.size()));
}

qreal PluginSlotsController::param(int slot, int index) const
{
    return slot >= 0 && slot < kSlots ? at(slot).engine.param(index) : 0.0;
}

void PluginSlotsController::setParam(int slot, int index, qreal value)
{
    if (slot < 0 || slot >= kSlots)
        return;
    at(slot).engine.setParam(index, float(value));
    m_dirty = true;
    m_saveTimer.start();
    bump();
}

void PluginSlotsController::poll()
{
    bool moved = false;
    for (int i = 0; i < kSlots; ++i) {
        Slot &s = at(i);
        if (!s.instance)
            continue;
        if (s.callback.exchange(false))
            s.instance->onMainThread();
        // Asked to restart (a new sample rate, its ports changed): stopped, then started again.
        if (s.restart.exchange(false) || s.instance->wantsRestart()) {
            const double rate = s.instance->wantsRestart() ? s.instance->restartRate() : rateFor(i);
            s.instance->deactivate();
            if (!s.instance->activate(rate, kMaxFrames))
                s.message = QStringLiteral("It wouldn't restart at %1 Hz.").arg(rate);
        }
        if (s.engine.takeParamsChangedByAudio())
            moved = true; // its own window turned a knob, or automation did
    }
    // Plugins taken out are freed once the audio thread has moved on (or isn't running).
    std::erase_if(m_retired, [this](const Retired &r) {
        return at(r.slot).engine.blocks() >= r.block + 2 || r.since.elapsed() > 1000;
    });
    if (moved)
        bump();
}

void PluginSlotsController::bump()
{
    ++m_revision;
    emit changed();
}

void PluginSlotsController::save()
{
    m_saveTimer.stop();
    QJsonArray saved;
    for (int i = 0; i < kSlots; ++i) {
        const Slot &s = at(i);
        if (!s.instance) {
            saved.append(QJsonValue());
            continue;
        }
        std::vector<std::uint8_t> state;
        s.instance->saveState(state);
        saved.append(QJsonObject{
            {QStringLiteral("path"), s.path},
            {QStringLiteral("id"), s.id},
            {QStringLiteral("state"), QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(state.data()),
                                                                     qsizetype(state.size()))
                                                              .toBase64())},
        });
    }
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile file(m_file);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("slots"), saved}}).toJson());
    file.commit();
    m_dirty = false;
}

void PluginSlotsController::restore()
{
    QFile file(m_file);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray saved = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("slots")).toArray();
    for (int i = 0; i < kSlots && i < saved.size(); ++i) {
        const QJsonObject o = saved[i].toObject();
        if (o.isEmpty())
            continue;
        if (!load(i, o.value(QStringLiteral("path")).toString(), o.value(QStringLiteral("id")).toString()))
            continue; // gone from the system: the slot stays empty, saying why
        const QByteArray bytes = QByteArray::fromBase64(o.value(QStringLiteral("state")).toString().toLatin1());
        if (!bytes.isEmpty())
            at(i).instance->loadState(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
    }
    bump();
}

QList<krate::InstrumentEntry> pluginSlotEntries(PluginSlotsController &owner)
{
    QList<krate::InstrumentEntry> entries;
    for (int i = 0; i < PluginSlotsController::kSlots; ++i) {
        krate::InstrumentEntry entry;
        entry.id = QStringLiteral("plugin%1").arg(i + 1);
        entry.name = QStringLiteral("Plugin %1").arg(i + 1);
        entry.playsNotes = true;
        entry.inRackByDefault = false;
        entry.qmlModule = QStringLiteral("KrateRack");
        entry.panelComponent = QStringLiteral("PluginPanel");
        entry.create = [&owner, i](const krate::InstrumentContext &) -> std::unique_ptr<krate::InstrumentInstance> {
            return std::make_unique<PluginSlotInstance>(owner.slot(i));
        };
        entries.append(entry);
    }
    return entries;
}
