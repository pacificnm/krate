#include "kalliope/app/SequenceController.h"

#include <algorithm>
#include <vector>

#include <QJSEngine>
#include <QLoggingCategory>

#include "kalliope/app/KalliopeController.h"
#include "kalliope/dsp/Engine.h"
#include "kalliope/dsp/Params.h"

Q_LOGGING_CATEGORY(lcSequences, "kalliope.sequences")

namespace {
constexpr int kSaveDelayMs = 2000;
constexpr int kMaxNotesPerStep = 4;

QString noteName(int note)
{
    static const char *const kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QString::fromLatin1(kNames[note % 12]) + QString::number(note / 12 - 1);
}
} // namespace

using namespace kalliope::persistence;

SequenceController::SequenceController(KalliopeController &panel, kalliope::Engine &engine, const QString &filePath,
                                       QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    switch (loadSequences(m_filePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcSequences) << "loaded sequences from" << m_filePath;
        break;
    case LoadResult::Missing:
        break;
    case LoadResult::Invalid:
        qCWarning(lcSequences) << "could not read" << m_filePath << "- starting with empty patterns";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcSequences) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    m_engine.setSequence(current()); // before processing starts

    connect(&m_panel, &krate::ParamController::paramsChanged, this, [this] {
        if (!active() && m_cursor >= 0) { // leaving SEQ ends recording
            m_cursor = -1;
            emit changed();
        }
        emit changed(); // active may have changed
    });
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &SequenceController::save);
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &SequenceController::poll);
    m_pollTimer.start();
}

SequenceController::~SequenceController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

SequenceController *SequenceController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool SequenceController::active() const
{
    return kalliope::units::index(float(m_panel.value(QStringLiteral("play_mode"))), kalliope::positions::kPlayMode) == 2;
}

void SequenceController::save()
{
    m_saveTimer.stop();
    if (!saveSequences(m_filePath, m_bank))
        qCWarning(lcSequences) << "could not save sequences to" << m_filePath;
}

void SequenceController::edited()
{
    m_engine.postSequence(current());
    ++m_revision;
    emit changed();
    m_saveTimer.start();
}

void SequenceController::setPatternIndex(int index)
{
    index = std::clamp(index, 0, kalliope::kSeqPatterns - 1);
    if (index == m_bank.current)
        return;
    m_bank.current = index;
    m_cursor = -1;
    edited();
}

int SequenceController::stepState(int step) const
{
    if (step < 0 || step >= 16)
        return 0;
    const kalliope::SeqStep &s = current().steps[std::size_t(step)];
    return s.tie ? 2 : s.count > 0 ? 1 : 0;
}

QString SequenceController::stepText(int step) const
{
    if (step < 0 || step >= 16)
        return {};
    const kalliope::SeqStep &s = current().steps[std::size_t(step)];
    if (s.tie)
        return QStringLiteral("tie");
    if (s.count == 0)
        return QStringLiteral("rest");
    QStringList names;
    for (int i = 0; i < s.count; ++i)
        names << noteName(s.notes[std::size_t(i)]);
    return names.join(u' ');
}

void SequenceController::pressStep(int step, bool shift)
{
    if (step < 0 || step >= 16)
        return;
    if (shift) {
        current().length = std::uint8_t(step + 1);
        if (m_cursor > step)
            m_cursor = step;
        edited();
        return;
    }
    m_cursor = m_cursor == step ? -1 : step;
    m_chord.clear();
    emit changed();
}

void SequenceController::advance()
{
    if (m_cursor >= 0)
        m_cursor = (m_cursor + 1) % current().length;
}

void SequenceController::recordStep(const QList<int> &notes)
{
    if (m_cursor < 0)
        return;
    kalliope::SeqStep &s = current().steps[std::size_t(m_cursor)];
    s = {};
    QList<int> sorted = notes;
    std::sort(sorted.begin(), sorted.end());
    for (int n : sorted)
        if (s.count < kMaxNotesPerStep)
            s.notes[std::size_t(s.count++)] = std::uint8_t(std::clamp(n, 0, 127));
    advance();
    edited();
}

void SequenceController::rest()
{
    recordStep({});
}

void SequenceController::tie()
{
    if (m_cursor < 0)
        return;
    current().steps[std::size_t(m_cursor)] = {};
    current().steps[std::size_t(m_cursor)].tie = true;
    advance();
    edited();
}

void SequenceController::clear()
{
    current() = kalliope::SeqPattern{};
    m_cursor = -1;
    edited();
}

QList<int> SequenceController::heldNotes() const
{
    QList<int> held;
    for (int n = 0; n < 128; ++n)
        if (m_engine.noteHeld(n))
            held << n;
    return held;
}

void SequenceController::generate()
{
    // Notes from the held keys, else from the pattern, else the default scale.
    std::vector<int> pool;
    for (int n : heldNotes())
        pool.push_back(n);
    if (pool.empty())
        for (int s = 0; s < current().length; ++s)
            for (int i = 0; i < current().steps[std::size_t(s)].count; ++i)
                pool.push_back(current().steps[std::size_t(s)].notes[std::size_t(i)]);
    const int octaves = 1 + kalliope::units::index(float(m_panel.value(QStringLiteral("arp_range"))),
                                                   kalliope::positions::kArpRange);
    m_seed = m_seed * 1664525u + 1013904223u; // a new pattern every press
    current() = kalliope::generatePattern(pool, octaves, m_panel.value(QStringLiteral("gen_density")),
                                          current().length, m_seed);
    m_cursor = -1;
    edited();
}

void SequenceController::poll()
{
    const int step = m_engine.sequencerStep();
    if (step != m_playingStep) {
        m_playingStep = step;
        emit playingStepChanged();
    }
    // Recording: gather the keys while any is down; when all are up, write them.
    if (m_cursor < 0 || !active() || m_panel.playing() || step >= 0) {
        m_chord.clear();
        return;
    }
    const QList<int> held = heldNotes();
    for (int n : held)
        if (!m_chord.contains(n))
            m_chord << n;
    if (held.isEmpty() && !m_chord.isEmpty()) {
        const QList<int> chord = m_chord;
        m_chord.clear();
        recordStep(chord);
    }
}
