#include "konnekt/app/KonnektController.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "konnekt/dsp/Modules.h"
#include "konnekt/dsp/PatchSpec.h"

Q_LOGGING_CATEGORY(lcKonnekt, "konnekt")

using namespace konnekt;

namespace {
constexpr int kFileVersion = 1;
constexpr int kSaveDelayMs = 2000;
constexpr int kCableColors = 8;

QString text(const char *s) { return QString::fromUtf8(s); }

QVariantList jacks(std::span<const JackInfo> list)
{
    QVariantList out;
    for (const JackInfo &j : list)
        out << QVariantMap{{QStringLiteral("key"), text(j.key)},      {QStringLiteral("name"), text(j.name)},
                           {QStringLiteral("signal"), int(j.signal)}, {QStringLiteral("help"), text(j.help)},
                           {QStringLiteral("knob"), j.knob},          {QStringLiteral("amount"), j.amount}};
    return out;
}

QString number(double v, int decimals) { return QString::number(v, 'f', decimals); }
} // namespace

KonnektController::KonnektController(Engine &engine, const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_path(dataDir + QStringLiteral("/current.json"))
    , m_patchesDir(dataDir + QStringLiteral("/patches"))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_seenKnobs.fill(-1.0f);

    QJsonObject json;
    if (krate::files::readVersionedJson(m_path, kFileVersion, &json) == krate::files::LoadResult::Loaded)
        fromJson(json);
    else { // a first patch: something to play
        // Side by side, 64 px per column of width and a small gap.
        qreal x = 16;
        const auto place = [&](const char *type) {
            const int slot = addModule(QString::fromLatin1(type), x, 16);
            x += findModule(type)->width * 64 + 16;
            return slot;
        };
        const int midi = place("midi_in");
        const int osc = place("oscillator");
        const int env = place("envelope");
        const int vca = place("vca");
        const int out = place("output");
        plug(midi, 0, osc, 0);
        plug(midi, 1, env, 0);
        plug(osc, 0, vca, 0);
        plug(env, 0, vca, 1);
        plug(vca, 0, out, 0);
    }

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &KonnektController::save);
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &KonnektController::poll);
    m_pollTimer.start();
}

KonnektController::~KonnektController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

KonnektController *KonnektController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList KonnektController::moduleTypes() const
{
    QVariantList out;
    for (const ModuleInfo *m : konnekt::moduleTypes())
        out << QVariantMap{{QStringLiteral("type"), text(m->type)},
                           {QStringLiteral("name"), text(m->name)},
                           {QStringLiteral("category"), text(m->category)},
                           {QStringLiteral("help"), text(m->help)}};
    return out;
}

void KonnektController::setVoices(int voices)
{
    voices = std::clamp(voices, 1, kVoices);
    if (voices == m_engine.voices())
        return;
    m_engine.setVoices(voices);
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void KonnektController::setHelp(const QString &help)
{
    if (help == m_help)
        return;
    m_help = help;
    emit helpChanged();
}

void KonnektController::setLearning(bool on)
{
    if (on == m_learning)
        return;
    m_learning = on;
    if (!on) {
        m_engine.cancelLearn();
        m_armedSlot = m_armedKnob = -1;
    }
    emit learnChanged();
}

const Module *KonnektController::module(int slot) const
{
    return slot >= 0 && slot < kMaxModules ? m_slots[std::size_t(slot)].module.get() : nullptr;
}

// --- Modules ---------------------------------------------------------------------------

int KonnektController::addModule(const QString &type, qreal x, qreal y)
{
    const ModuleInfo *info = findModule(type.toStdString());
    if (!info)
        return -1;
    int slot = 0;
    while (slot < kMaxModules && m_slots[std::size_t(slot)].module)
        ++slot;
    if (slot == kMaxModules)
        return -1;
    auto m = std::unique_ptr<Module>(info->create());
    m->prepare(m_engine.sampleRate());
    m_engine.addModule(slot, m.get());
    m_slots[std::size_t(slot)] = {std::move(m), std::max<qreal>(0, x), std::max<qreal>(0, y)};
    ++m_moduleRevision;
    emit modulesChanged();
    structureChanged();
    return slot;
}

void KonnektController::removeModule(int slot)
{
    if (!module(slot))
        return;
    std::erase_if(m_cables, [slot](const Cable &c) { return c.fromSlot == slot || c.toSlot == slot; });
    m_engine.removeModule(slot);
    m_engine.forgetSlot(slot);
    m_leaving.push_back(std::move(m_slots[std::size_t(slot)].module));
    m_slots[std::size_t(slot)] = {};
    if (m_scopeSlot == slot)
        scope(-1, -1);
    ++m_moduleRevision;
    emit modulesChanged();
    structureChanged();
}

void KonnektController::moveModule(int slot, qreal x, qreal y)
{
    if (!module(slot))
        return;
    m_slots[std::size_t(slot)].x = std::max<qreal>(0, x);
    m_slots[std::size_t(slot)].y = std::max<qreal>(0, y);
    m_saveTimer.start();
}

QVariantList KonnektController::modules() const
{
    QVariantList out;
    for (int s = 0; s < kMaxModules; ++s)
        if (const Module *m = module(s))
            out << QVariantMap{{QStringLiteral("slot"), s},
                               {QStringLiteral("type"), text(m->info().type)},
                               {QStringLiteral("x"), m_slots[std::size_t(s)].x},
                               {QStringLiteral("y"), m_slots[std::size_t(s)].y}};
    return out;
}

QVariantMap KonnektController::moduleInfo(const QString &type) const
{
    const ModuleInfo *info = findModule(type.toStdString());
    if (!info)
        return {};
    QVariantList knobs;
    for (const KnobInfo &k : info->knobs) {
        QStringList labels;
        for (const char *l : k.labels)
            labels << text(l);
        knobs << QVariantMap{{QStringLiteral("key"), text(k.key)},   {QStringLiteral("name"), text(k.name)},
                             {QStringLiteral("detents"), k.detents}, {QStringLiteral("labels"), labels},
                             {QStringLiteral("help"), text(k.help)}};
    }
    return {{QStringLiteral("type"), text(info->type)},   {QStringLiteral("name"), text(info->name)},
            {QStringLiteral("help"), text(info->help)},   {QStringLiteral("category"), text(info->category)},
            {QStringLiteral("perVoice"), info->perVoice}, {QStringLiteral("width"), info->width},
            {QStringLiteral("knobs"), knobs},             {QStringLiteral("inputs"), jacks(info->inputs)},
            {QStringLiteral("outputs"), jacks(info->outputs)}};
}

void KonnektController::clearPatch()
{
    for (int s = 0; s < kMaxModules; ++s)
        if (module(s))
            removeModule(s);
    m_nextColor = 0;
}

// --- Knobs ---------------------------------------------------------------------------------

qreal KonnektController::knob(int slot, int k) const
{
    const Module *m = module(slot);
    return m && k >= 0 && k < int(m->info().knobs.size()) ? m->knobPosition(k) : 0.0;
}

void KonnektController::setKnob(int slot, int k, qreal position)
{
    Module *m = slot >= 0 && slot < kMaxModules ? m_slots[std::size_t(slot)].module.get() : nullptr;
    if (!m || k < 0 || k >= int(m->info().knobs.size()))
        return;
    m->setKnobPosition(k, float(position));
    ++m_knobRevision;
    emit knobsChanged();
    m_saveTimer.start();
}

qreal KonnektController::knobDefault(int slot, int k) const
{
    const Module *m = module(slot);
    if (!m || k < 0 || k >= int(m->info().knobs.size()))
        return 0.0;
    const KnobInfo &info = m->info().knobs[std::size_t(k)];
    return info.position(info.defaultValue);
}

QString KonnektController::knobText(int slot, int k, qreal position) const
{
    const Module *m = module(slot);
    if (!m || k < 0 || k >= int(m->info().knobs.size()))
        return {};
    const KnobInfo &info = m->info().knobs[std::size_t(k)];
    if (!info.labels.empty())
        return text(info.labels[std::size_t(info.index(float(position)))]);
    const double v = info.value(float(position));
    const std::string_view unit = info.unit;
    if (unit == "Hz")
        return v >= 1000.0 ? number(v / 1000.0, 2) + QStringLiteral(" kHz") : number(v, v < 10.0 ? 2 : 0) + QStringLiteral(" Hz");
    if (unit == "s")
        return v < 1.0 ? number(v * 1000.0, v < 0.01 ? 1 : 0) + QStringLiteral(" ms") : number(v, 2) + QStringLiteral(" s");
    if (unit == "st")
        return (v > 0.0 ? QStringLiteral("+") : QString()) + number(v, info.detents > 1 ? 0 : 1) + QStringLiteral(" st");
    if (unit == "oct")
        return (info.detents > 1 && v > 0.0 ? QStringLiteral("+") : QString()) + number(v, info.detents > 1 ? 0 : 2) + QStringLiteral(" oct");
    if (unit == "BPM")
        return number(v, 0) + QStringLiteral(" BPM");
    if (info.detents > 1)
        return number(v, 0);
    if (info.min >= 0.0f && info.max <= 1.0f)
        return number(v * 100.0, 0) + u'%';
    return number(v, 2);
}

// --- Cables ---------------------------------------------------------------------------------

QVariantList KonnektController::cables() const
{
    QVariantList out;
    for (const Cable &c : m_cables)
        out << QVariantMap{{QStringLiteral("fromSlot"), c.fromSlot}, {QStringLiteral("fromJack"), c.fromJack},
                           {QStringLiteral("toSlot"), c.toSlot},     {QStringLiteral("toJack"), c.toJack},
                           {QStringLiteral("color"), c.color}};
    return out;
}

bool KonnektController::plug(int outSlot, int outJack, int inSlot, int inJack)
{
    const Module *from = module(outSlot), *to = module(inSlot);
    if (!from || !to || outJack < 0 || outJack >= int(from->info().outputs.size()) || inJack < 0
        || inJack >= int(to->info().inputs.size()))
        return false;
    std::erase_if(m_cables, [&](const Cable &c) { return c.toSlot == inSlot && c.toJack == inJack; });
    m_cables.push_back({outSlot, outJack, inSlot, inJack, m_nextColor});
    m_nextColor = (m_nextColor + 1) % kCableColors;
    m_engine.connect(inSlot, inJack, outSlot, outJack);
    structureChanged();
    return true;
}

void KonnektController::unplug(int inSlot, int inJack)
{
    const auto before = m_cables.size();
    std::erase_if(m_cables, [&](const Cable &c) { return c.toSlot == inSlot && c.toJack == inJack; });
    if (m_cables.size() == before)
        return;
    m_engine.disconnect(inSlot, inJack);
    structureChanged();
}

QVariantMap KonnektController::cableInto(int inSlot, int inJack) const
{
    for (const Cable &c : m_cables)
        if (c.toSlot == inSlot && c.toJack == inJack)
            return {{QStringLiteral("fromSlot"), c.fromSlot}, {QStringLiteral("fromJack"), c.fromJack}};
    return {};
}

void KonnektController::structureChanged()
{
    std::vector<int> present;
    for (int s = 0; s < kMaxModules; ++s)
        if (module(s))
            present.push_back(s);
    std::vector<Engine::Cable> cables;
    for (const Cable &c : m_cables)
        cables.push_back({c.fromSlot, c.fromJack, c.toSlot, c.toJack});
    m_engine.setOrder(Engine::signalOrder(present, cables));
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

// --- Seeing signals -------------------------------------------------------------------------

QVariantList KonnektController::trace(int slot, int jack) const
{
    const Trace t = m_engine.trace(slot, jack);
    QVariantList out;
    out.reserve(2 * kTracePoints);
    for (float v : t.low)
        out << v;
    for (float v : t.high)
        out << v;
    return out;
}

void KonnektController::scope(int slot, int jack)
{
    m_scopeSlot = module(slot) ? slot : -1;
    m_scopeJack = m_scopeSlot >= 0 ? jack : -1;
    m_engine.setProbe(m_scopeSlot, m_scopeJack);
    ++m_revision;
    emit changed();
}

QString KonnektController::scopeName() const
{
    const Module *m = module(m_scopeSlot);
    if (!m || m_scopeJack < 0 || m_scopeJack >= int(m->info().outputs.size()))
        return {};
    return text(m->info().name) + QStringLiteral(" · ") + text(m->info().outputs[std::size_t(m_scopeJack)].name);
}

QVariantList KonnektController::scopeSamples(int count) const
{
    std::vector<float> x(std::size_t(std::clamp(count, 1, kProbeSamples)));
    m_engine.probe(x);
    QVariantList out;
    out.reserve(qsizetype(x.size()));
    for (float v : x)
        out << v;
    return out;
}

// --- MIDI Learn ------------------------------------------------------------------------------

void KonnektController::armLearn(int slot, int k)
{
    m_armedSlot = slot;
    m_armedKnob = k;
    m_engine.armLearn(slot, k);
    emit learnChanged();
}

int KonnektController::learnedCc(int slot, int k) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (const auto m = m_engine.ccMapping(cc); m && m->first == slot && m->second == k)
            return cc;
    return -1;
}

void KonnektController::forget(int slot, int k)
{
    for (int cc = 0; cc < 128; ++cc)
        if (const auto m = m_engine.ccMapping(cc); m && m->first == slot && m->second == k)
            m_engine.setCcMapping(cc, -1, -1);
    emit learnChanged();
    m_saveTimer.start();
}

// --- Polling ---------------------------------------------------------------------------------

void KonnektController::poll()
{
    // Modules the engine has let go: free them.
    while (Module *gone = m_engine.popRetired())
        std::erase_if(m_leaving, [gone](const std::unique_ptr<Module> &m) { return m.get() == gone; });
    // A CC learned.
    if (const auto learned = m_engine.popLearned()) {
        (void)learned;
        m_armedSlot = m_armedKnob = -1;
        emit learnChanged();
        m_saveTimer.start();
    }
    // Knobs moved by MIDI: the panel follows.
    bool moved = false;
    for (int s = 0; s < kMaxModules; ++s)
        if (const Module *m = module(s))
            for (int k = 0; k < int(m->info().knobs.size()); ++k) {
                float &seen = m_seenKnobs[std::size_t(s * kMaxKnobs + k)];
                if (const float p = m->knobPosition(k); p != seen) {
                    moved = moved || seen >= 0.0f;
                    seen = p;
                }
            }
    if (moved) {
        ++m_knobRevision;
        emit knobsChanged();
        m_saveTimer.start();
    }
    ++m_frame;
    emit frameChanged();
}

// --- JSON ----------------------------------------------------------------------------------------

// --- Saved patches and lessons ---------------------------------------------------------------

namespace {
// A name that's safe as a file name: no slashes, not empty, not too long.
QString cleanName(const QString &name)
{
    QString n = name.trimmed().left(48);
    n.replace(u'/', u'-');
    n.replace(u'\\', u'-');
    return n == QStringLiteral(".") || n == QStringLiteral("..") ? QString() : n;
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
} // namespace

QStringList KonnektController::patchNames() const
{
    QStringList names;
    for (const QString &f : QDir(m_patchesDir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name | QDir::IgnoreCase))
        names << f.chopped(5);
    return names;
}

bool KonnektController::savePatch(const QString &name)
{
    const QString n = cleanName(name);
    if (n.isEmpty())
        return false;
    QDir().mkpath(m_patchesDir);
    QJsonObject json = toJson();
    json.insert(QStringLiteral("name"), n);
    if (!krate::files::writeJsonFile(m_patchesDir + u'/' + n + QStringLiteral(".json"), json))
        return false;
    m_patchName = n;
    emit patchesChanged();
    return true;
}

bool KonnektController::loadPatch(const QString &name)
{
    const QString n = cleanName(name);
    if (n.isEmpty() || !fromJson(readJson(m_patchesDir + u'/' + n + QStringLiteral(".json"))))
        return false;
    m_patchName = n;
    m_lesson = -1;
    emit patchesChanged();
    return true;
}

void KonnektController::deletePatch(const QString &name)
{
    const QString n = cleanName(name);
    if (!n.isEmpty() && QFile::remove(m_patchesDir + u'/' + n + QStringLiteral(".json"))) {
        if (m_patchName == n)
            m_patchName.clear();
        emit patchesChanged();
    }
}

QVariantList KonnektController::lessons() const
{
    QVariantList out;
    const QStringList files = QDir(m_lessonsDir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (qsizetype i = 0; i < files.size(); ++i)
        out << QVariantMap{{QStringLiteral("index"), int(i)},
                           {QStringLiteral("title"), readJson(m_lessonsDir + u'/' + files[i]).value(u"title").toString()}};
    return out;
}

bool KonnektController::loadLesson(int index)
{
    const QStringList files = QDir(m_lessonsDir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (index < 0 || index >= files.size())
        return false;
    const QJsonObject json = readJson(m_lessonsDir + u'/' + files[index]);
    if (!fromJson(json))
        return false;
    m_lesson = index;
    m_lessonTitle = QStringLiteral("%1 · %2").arg(index + 1).arg(json.value(u"title").toString());
    m_lessonText = json.value(u"text").toString();
    m_patchName.clear();
    emit patchesChanged();
    return true;
}

void KonnektController::closeLesson()
{
    m_lesson = -1;
    emit patchesChanged();
}

QJsonObject KonnektController::toJson() const
{
    QJsonArray modules;
    for (int s = 0; s < kMaxModules; ++s)
        if (const Module *m = module(s)) {
            QJsonObject knobs;
            for (std::size_t k = 0; k < m->info().knobs.size(); ++k)
                knobs.insert(text(m->info().knobs[k].key), double(m->knob(int(k))));
            modules.append(QJsonObject{{QStringLiteral("slot"), s},
                                       {QStringLiteral("type"), text(m->info().type)},
                                       {QStringLiteral("x"), m_slots[std::size_t(s)].x},
                                       {QStringLiteral("y"), m_slots[std::size_t(s)].y},
                                       {QStringLiteral("knobs"), knobs}});
        }
    QJsonArray cables;
    for (const Cable &c : m_cables)
        cables.append(QJsonObject{{QStringLiteral("from"), c.fromSlot},
                                  {QStringLiteral("out"), text(module(c.fromSlot)->info().outputs[std::size_t(c.fromJack)].key)},
                                  {QStringLiteral("to"), c.toSlot},
                                  {QStringLiteral("in"), text(module(c.toSlot)->info().inputs[std::size_t(c.toJack)].key)},
                                  {QStringLiteral("color"), c.color}});
    QJsonArray midi;
    for (int cc = 0; cc < 128; ++cc)
        if (const auto m = m_engine.ccMapping(cc); m && module(m->first))
            midi.append(QJsonObject{{QStringLiteral("cc"), cc},
                                    {QStringLiteral("slot"), m->first},
                                    {QStringLiteral("knob"), text(module(m->first)->info().knobs[std::size_t(m->second)].key)}});
    return {{QStringLiteral("version"), kFileVersion}, {QStringLiteral("voices"), m_engine.voices()},
            {QStringLiteral("modules"), modules},      {QStringLiteral("cables"), cables},
            {QStringLiteral("midi"), midi}};
}

bool KonnektController::fromJson(const QJsonObject &json)
{
    if (!json.contains(u"modules"))
        return false;
    clearPatch();
    for (int cc = 0; cc < 128; ++cc)
        m_engine.setCcMapping(cc, -1, -1);
    m_engine.setVoices(json.value(u"voices").toInt(4));
    for (const QJsonValue v : json.value(u"modules").toArray()) {
        const QJsonObject o = v.toObject();
        const int slot = o.value(u"slot").toInt(-1);
        const ModuleInfo *info = findModule(o.value(u"type").toString().toStdString());
        if (!info || slot < 0 || slot >= kMaxModules || m_slots[std::size_t(slot)].module)
            continue;
        auto m = std::unique_ptr<Module>(info->create());
        const QJsonObject knobs = o.value(u"knobs").toObject();
        for (std::size_t k = 0; k < info->knobs.size(); ++k)
            if (const QJsonValue kv = knobs.value(text(info->knobs[k].key)); kv.isDouble())
                m->setKnobPosition(int(k), info->knobs[k].position(float(kv.toDouble())));
        m->prepare(m_engine.sampleRate());
        m_engine.addModule(slot, m.get());
        m_slots[std::size_t(slot)] = {std::move(m), o.value(u"x").toDouble(), o.value(u"y").toDouble()};
    }
    ++m_moduleRevision;
    emit modulesChanged();
    for (const QJsonValue v : json.value(u"cables").toArray()) {
        const QJsonObject o = v.toObject();
        const int from = o.value(u"from").toInt(-1), to = o.value(u"to").toInt(-1);
        const Module *a = module(from), *b = module(to);
        if (!a || !b)
            continue;
        const int out = outputIndex(a->info(), o.value(u"out").toString().toStdString());
        const int in = inputIndex(b->info(), o.value(u"in").toString().toStdString());
        if (out < 0 || in < 0)
            continue;
        m_cables.push_back({from, out, to, in, o.value(u"color").toInt(m_nextColor) % kCableColors});
        m_nextColor = (m_cables.back().color + 1) % kCableColors;
        m_engine.connect(to, in, from, out);
    }
    for (const QJsonValue v : json.value(u"midi").toArray()) {
        const QJsonObject o = v.toObject();
        const int slot = o.value(u"slot").toInt(-1);
        if (const Module *m = module(slot))
            if (const int k = knobIndex(m->info(), o.value(u"knob").toString().toStdString()); k >= 0)
                m_engine.setCcMapping(o.value(u"cc").toInt(-1), slot, k);
    }
    structureChanged();
    ++m_knobRevision;
    emit knobsChanged();
    emit learnChanged();
    return true;
}

void KonnektController::save()
{
    m_saveTimer.stop();
    if (!krate::files::writeJsonFile(m_path, toJson()))
        qCWarning(lcKonnekt) << "could not save" << m_path;
}
