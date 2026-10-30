#include "kobalt/app/KobaltSequenceController.h"

#include <algorithm>

#include <QJSEngine>
#include <QLoggingCategory>

#include "kobalt/app/KobaltController.h"
#include "kobalt/dsp/Engine.h"
#include "kobalt/dsp/Params.h"

Q_LOGGING_CATEGORY(lcKobaltSequences, "kobalt.sequences")

namespace {
constexpr int kSaveDelayMs = 2000;

QString noteName(int note)
{
    static const char *const kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QString::fromLatin1(kNames[note % 12]) + QString::number(note / 12 - 1);
}
} // namespace

using namespace kobalt::persistence;
using Kind = kobalt::SeqStep::Kind;

KobaltSequenceController::KobaltSequenceController(KobaltController &panel, kobalt::Engine &engine,
                                                   const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    switch (loadSequences(m_filePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcKobaltSequences) << "loaded sequences from" << m_filePath;
        break;
    case LoadResult::Missing:
        break;
    case LoadResult::Invalid:
        qCWarning(lcKobaltSequences) << "could not read" << m_filePath << "- starting with empty patterns";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcKobaltSequences) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    m_engine.setSequence(current()); // before processing starts

    connect(&m_panel, &krate::ParamController::paramsChanged, this, [this] {
        if (!active() && m_cursor >= 0) // leaving SEQ ends loading
            m_cursor = -1;
        emit changed(); // active may have changed
    });
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &KobaltSequenceController::save);
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &KobaltSequenceController::poll);
    m_pollTimer.start();
}

KobaltSequenceController::~KobaltSequenceController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

KobaltSequenceController *KobaltSequenceController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool KobaltSequenceController::active() const
{
    return kobalt::units::index(float(m_panel.value(QStringLiteral("play_mode"))), kobalt::positions::kPlayMode) == 2;
}

void KobaltSequenceController::save()
{
    m_saveTimer.stop();
    if (!saveSequences(m_filePath, m_bank))
        qCWarning(lcKobaltSequences) << "could not save sequences to" << m_filePath;
}

void KobaltSequenceController::edited()
{
    m_engine.postSequence(current());
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void KobaltSequenceController::setPatternIndex(int index)
{
    index = std::clamp(index, 0, kobalt::kSeqPatterns - 1);
    if (index == m_bank.current)
        return;
    m_bank.current = index;
    m_cursor = -1;
    edited();
}

int KobaltSequenceController::stepState(int step) const
{
    if (step < 0 || step >= kobalt::kSeqMaxSteps)
        return 0;
    switch (current().steps[std::size_t(step)].kind) {
    case Kind::Rest: return 0;
    case Kind::Note: return 1;
    case Kind::Tie: return 2;
    }
    return 0;
}

bool KobaltSequenceController::stepLegato(int step) const
{
    return step >= 0 && step < kobalt::kSeqMaxSteps && current().steps[std::size_t(step)].legato
           && current().steps[std::size_t(step)].kind != Kind::Rest;
}

QString KobaltSequenceController::stepText(int step) const
{
    if (step < 0 || step >= kobalt::kSeqMaxSteps)
        return {};
    const kobalt::SeqStep &s = current().steps[std::size_t(step)];
    QString text;
    switch (s.kind) {
    case Kind::Rest: return QStringLiteral("rest");
    case Kind::Note: text = noteName(s.note); break;
    case Kind::Tie: text = QStringLiteral("tie"); break;
    }
    return s.legato ? text + QStringLiteral(" legato") : text;
}

void KobaltSequenceController::pressStep(int step, bool shift)
{
    if (step < 0 || step >= kobalt::kSeqMaxSteps)
        return;
    if (shift) {
        current().length = std::uint8_t(step + 1);
        if (m_cursor > step)
            m_cursor = step;
        edited();
        return;
    }
    m_cursor = m_cursor == step ? -1 : step;
    m_lastLoaded = -1;
    emit changed();
}

void KobaltSequenceController::advance()
{
    if (m_cursor >= 0)
        m_cursor = (m_cursor + 1) % current().length;
}

int KobaltSequenceController::previous() const
{
    if (m_cursor < 0)
        return -1;
    return (m_cursor + current().length - 1) % current().length;
}

void KobaltSequenceController::loadNote(int note, bool slur)
{
    if (m_cursor < 0)
        return;
    if (slur && previous() >= 0 && current().steps[std::size_t(previous())].kind != Kind::Rest)
        current().steps[std::size_t(previous())].legato = true;
    current().steps[std::size_t(m_cursor)] = {Kind::Note, std::uint8_t(std::clamp(note, 0, 127)), false};
    advance();
    edited();
}

void KobaltSequenceController::rest()
{
    if (m_cursor < 0)
        return;
    current().steps[std::size_t(m_cursor)] = {};
    m_lastLoaded = -1;
    advance();
    edited();
}

void KobaltSequenceController::tie()
{
    if (m_cursor < 0)
        return;
    current().steps[std::size_t(m_cursor)] = {Kind::Tie, 60, false};
    advance();
    edited();
}

void KobaltSequenceController::legato()
{
    const int step = previous();
    if (step < 0 || current().steps[std::size_t(step)].kind == Kind::Rest)
        return;
    current().steps[std::size_t(step)].legato = !current().steps[std::size_t(step)].legato;
    edited();
}

void KobaltSequenceController::clear()
{
    current() = kobalt::SeqPattern{};
    m_cursor = -1;
    m_lastLoaded = -1;
    edited();
}

void KobaltSequenceController::poll()
{
    const int step = m_engine.sequencerStep();
    if (step != m_playingStep) {
        m_playingStep = step;
        emit playingStepChanged();
    }
    QList<int> held;
    for (int n = 0; n < 128; ++n)
        if (m_engine.noteHeld(n))
            held << n;
    // Loading: each key going down is a step (low to high if several at once).
    if (m_cursor >= 0 && active() && !m_panel.playing() && step < 0) {
        for (int n : held)
            if (!m_held.contains(n)) {
                loadNote(n, m_lastLoaded >= 0 && held.contains(m_lastLoaded));
                m_lastLoaded = n;
            }
    }
    m_held = held;
}
