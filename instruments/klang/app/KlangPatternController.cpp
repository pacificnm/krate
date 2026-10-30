#include "klang/app/KlangPatternController.h"

#include <algorithm>

#include <QJSEngine>
#include <QJsonArray>
#include <QLoggingCategory>

#include "app/JsonFile.h"
#include "klang/dsp/Engine.h"

Q_LOGGING_CATEGORY(lcKlangPatterns, "klang.patterns")

namespace {
constexpr int kFileVersion = 1;
constexpr int kSaveDelayMs = 2000;
} // namespace

using klang::kMaxSteps;
using klang::kPatternPads;
using klang::kPatterns;

KlangPatternController::KlangPatternController(klang::Engine &engine, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    QJsonObject json;
    switch (krate::files::readVersionedJson(m_filePath, kFileVersion, &json)) {
    case krate::files::LoadResult::Loaded:
        m_bank = fromJson(json, &m_current);
        break;
    case krate::files::LoadResult::Invalid:
        qCWarning(lcKlangPatterns) << "could not read" << m_filePath << "- starting with empty patterns";
        break;
    case krate::files::LoadResult::NewerVersion:
        m_filePath += QStringLiteral(".v1");
        break;
    case krate::files::LoadResult::Missing:
        break;
    }
    for (int i = 0; i < kPatterns; ++i) // before processing starts
        m_engine.setPattern(i, m_bank[std::size_t(i)]);
    m_engine.cuePattern(m_current);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &KlangPatternController::save);
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &KlangPatternController::poll);
    m_pollTimer.start();
}

KlangPatternController::~KlangPatternController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

KlangPatternController *KlangPatternController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void KlangPatternController::poll()
{
    // A pattern cued by Program Change (or a learned button) shows up here too.
    const int playing = m_engine.playingPattern(), step = m_engine.playingStep();
    if (playing != m_playing || step != m_playingStep) {
        m_playing = playing;
        m_playingStep = step;
        emit playingChanged();
    }
}

void KlangPatternController::edited()
{
    m_engine.postPattern(m_current, pattern());
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void KlangPatternController::select(int index)
{
    index = std::clamp(index, 0, kPatterns - 1);
    m_engine.cuePattern(index);
    if (index == m_current)
        return;
    m_current = index;
    m_page = std::min(m_page, (pattern().length - 1) / 16);
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void KlangPatternController::setLength(int length)
{
    length = std::clamp(length, 1, kMaxSteps);
    if (length == pattern().length)
        return;
    pattern().length = std::uint8_t(length);
    m_page = std::min(m_page, (length - 1) / 16);
    edited();
}

void KlangPatternController::setPage(int page)
{
    page = std::clamp(page, 0, 3);
    if (page == m_page)
        return;
    m_page = page;
    emit changed();
}

void KlangPatternController::setTool(int tool)
{
    tool = std::clamp(tool, int(Note), int(PitchDown));
    if (tool == m_tool)
        return;
    m_tool = tool;
    emit changed();
}

klang::Step *KlangPatternController::step(int pad, int s)
{
    return pad >= 0 && pad < kPatternPads && s >= 0 && s < kMaxSteps ? &pattern().steps[std::size_t(pad)][std::size_t(s)]
                                                                     : nullptr;
}

const klang::Step *KlangPatternController::step(int pad, int s) const
{
    return pad >= 0 && pad < kPatternPads && s >= 0 && s < kMaxSteps ? &pattern().steps[std::size_t(pad)][std::size_t(s)]
                                                                     : nullptr;
}

bool KlangPatternController::stepOn(int pad, int s) const { const auto *st = step(pad, s); return st && st->on; }
bool KlangPatternController::stepAccent(int pad, int s) const { const auto *st = step(pad, s); return st && st->on && st->accent; }
int KlangPatternController::stepRepeat(int pad, int s) const { const auto *st = step(pad, s); return st ? st->repeat : 1; }
bool KlangPatternController::stepReverse(int pad, int s) const { const auto *st = step(pad, s); return st && st->on && st->reverse; }
int KlangPatternController::stepPitch(int pad, int s) const { const auto *st = step(pad, s); return st ? st->pitch : 0; }

void KlangPatternController::pressStep(int pad, int s)
{
    klang::Step *st = step(pad, s);
    if (!st)
        return;
    switch (m_tool) {
    case Note:
        if (st->on)
            *st = {}; // off, and back to a plain step
        else
            st->on = true;
        break;
    case Accent:
        st->on = true;
        st->accent = !st->accent;
        break;
    case Roll: // 1 → 2 → 3 → 4 → 1
        st->on = true;
        st->repeat = std::uint8_t(st->repeat % klang::kMaxRepeat + 1);
        break;
    case Reverse:
        st->on = true;
        st->reverse = !st->reverse;
        break;
    case PitchUp:
        st->on = true;
        st->pitch = std::int8_t(std::min(12, st->pitch + 1));
        break;
    case PitchDown:
        st->on = true;
        st->pitch = std::int8_t(std::max(-12, st->pitch - 1));
        break;
    }
    edited();
}

bool KlangPatternController::padUsed(int pad) const
{
    if (pad < 0 || pad >= kPatternPads)
        return false;
    for (int s = 0; s < pattern().length; ++s)
        if (pattern().steps[std::size_t(pad)][std::size_t(s)].on)
            return true;
    return false;
}

bool KlangPatternController::used(int index) const
{
    if (index < 0 || index >= kPatterns)
        return false;
    const klang::Pattern &p = m_bank[std::size_t(index)];
    for (const auto &pad : p.steps)
        for (int s = 0; s < p.length; ++s)
            if (pad[std::size_t(s)].on)
                return true;
    return false;
}

bool KlangPatternController::gateStep(int s) const
{
    return s >= 0 && s < 16 && ((pattern().gate >> s) & 1u);
}

void KlangPatternController::toggleGateStep(int s)
{
    if (s < 0 || s >= 16)
        return;
    pattern().gate = std::uint16_t(pattern().gate ^ (1u << s));
    edited();
}

void KlangPatternController::clear()
{
    pattern() = klang::Pattern{};
    m_page = 0;
    edited();
}

void KlangPatternController::clearPad(int pad)
{
    if (pad < 0 || pad >= kPatternPads)
        return;
    pattern().steps[std::size_t(pad)] = {};
    edited();
}

void KlangPatternController::copy()
{
    m_copy = pattern();
    emit changed();
}

void KlangPatternController::paste()
{
    if (!m_copy)
        return;
    pattern() = *m_copy;
    edited();
}

// --- patterns.json ----------------------------------------------------------------------

QJsonObject KlangPatternController::toJson(const std::array<klang::Pattern, kPatterns> &bank, int current)
{
    QJsonArray patterns;
    for (const klang::Pattern &p : bank) {
        // Each pad's steps that are on: [step, accent, repeat, reverse, pitch].
        QJsonArray pads;
        for (const auto &pad : p.steps) {
            QJsonArray steps;
            for (int s = 0; s < p.length; ++s)
                if (const klang::Step &st = pad[std::size_t(s)]; st.on)
                    steps.append(QJsonArray{s, st.accent ? 1 : 0, int(st.repeat), st.reverse ? 1 : 0, int(st.pitch)});
            pads.append(steps);
        }
        patterns.append(QJsonObject{{QStringLiteral("length"), p.length},
                                    {QStringLiteral("gate"), int(p.gate)},
                                    {QStringLiteral("pads"), pads}});
    }
    return {{QStringLiteral("version"), kFileVersion},
            {QStringLiteral("current"), current},
            {QStringLiteral("patterns"), patterns}};
}

std::array<klang::Pattern, kPatterns> KlangPatternController::fromJson(const QJsonObject &json, int *current)
{
    std::array<klang::Pattern, kPatterns> bank{};
    *current = std::clamp(json.value(u"current").toInt(0), 0, kPatterns - 1);
    const QJsonArray patterns = json.value(u"patterns").toArray();
    for (int i = 0; i < kPatterns && i < patterns.size(); ++i) {
        const QJsonObject po = patterns.at(i).toObject();
        klang::Pattern &p = bank[std::size_t(i)];
        p.length = std::uint8_t(std::clamp(po.value(u"length").toInt(16), 1, kMaxSteps));
        p.gate = std::uint16_t(po.value(u"gate").toInt(klang::kDefaultGate) & 0xFFFF);
        const QJsonArray pads = po.value(u"pads").toArray();
        for (int pad = 0; pad < kPatternPads && pad < pads.size(); ++pad)
            for (const QJsonValue v : pads.at(pad).toArray()) {
                const QJsonArray a = v.toArray();
                const int s = a.at(0).toInt(-1);
                if (s < 0 || s >= p.length)
                    continue;
                klang::Step &st = p.steps[std::size_t(pad)][std::size_t(s)];
                st.on = true;
                st.accent = a.at(1).toInt(0) != 0;
                st.repeat = std::uint8_t(std::clamp(a.at(2).toInt(1), 1, klang::kMaxRepeat));
                st.reverse = a.at(3).toInt(0) != 0;
                st.pitch = std::int8_t(std::clamp(a.at(4).toInt(0), -12, 12));
            }
    }
    return bank;
}

void KlangPatternController::save()
{
    m_saveTimer.stop();
    if (!krate::files::writeJsonFile(m_filePath, toJson(m_bank, m_current)))
        qCWarning(lcKlangPatterns) << "could not save" << m_filePath;
}
