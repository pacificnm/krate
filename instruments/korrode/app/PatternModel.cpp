#include "app/PatternModel.h"

#include <algorithm>

#include <QJSEngine>
#include <QLoggingCategory>
#include <QRandomGenerator>

#include "dsp/Engine.h"

Q_LOGGING_CATEGORY(lcPattern, "korrode.pattern")

using korrode::Command;
using korrode::Gate;
using korrode::Step;

namespace {

constexpr int kSaveDelayMs = 2000;
constexpr int kAuditionBaseNote = 36; // C2: the bass range of the default octave

QString noteName(int semitone)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"};
    return QString::fromLatin1(names[std::clamp(semitone, 0, 12)]);
}

} // namespace

PatternModel::PatternModel(korrode::Engine &engine, const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    using korrode::persistence::LoadResult;
    switch (korrode::persistence::loadPatterns(m_filePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcPattern) << "loaded patterns from" << m_filePath;
        break;
    case LoadResult::Missing:
        qCInfo(lcPattern) << "no pattern file yet; using defaults";
        break;
    case LoadResult::Invalid:
        qCWarning(lcPattern) << "could not read" << m_filePath << "- using defaults";
        break;
    case LoadResult::NewerVersion:
        // Leave the newer file alone; this version saves next to it instead.
        qCWarning(lcPattern) << m_filePath << "was written by a newer korrode; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &PatternModel::save);

    sendCurrentPattern();
}

PatternModel::~PatternModel()
{
    if (m_dirty)
        save();
    s_instance = nullptr;
}

PatternModel *PatternModel::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int PatternModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(current().steps.size());
}

QVariant PatternModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const Step &s = current().steps[std::size_t(index.row())];
    switch (role) {
    case NoteRole: return s.note;
    case OctaveRole: return s.octave;
    case AccentRole: return s.accent;
    case SlideRole: return s.slide;
    case GateRole: return int(s.gate);
    case ActiveRole: return index.row() < current().length;
    case LabelRole:
        switch (s.gate) {
        case Gate::Note:
            return noteName(s.note) + (s.octave > 0 ? QStringLiteral("↑") : s.octave < 0 ? QStringLiteral("↓") : QString());
        case Gate::Tie: return QStringLiteral("–");
        case Gate::Rest: return QString();
        }
    }
    return {};
}

QHash<int, QByteArray> PatternModel::roleNames() const
{
    return {
        {NoteRole, "note"},
        {OctaveRole, "octave"},
        {AccentRole, "accent"},
        {SlideRole, "slide"},
        {GateRole, "gate"},
        {ActiveRole, "active"},
        {LabelRole, "label"},
    };
}

QString PatternModel::name() const
{
    return QChar(u'A' + bank()) + QString::number(number() + 1);
}

void PatternModel::pressBank(int bank)
{
    if (bank < 0 || bank >= korrode::persistence::kBanks)
        return;
    m_pendingBank = m_pendingBank == bank ? -1 : bank; // again: never mind
    emit indexChanged();
}

void PatternModel::pressNumber(int number)
{
    if (number < 0 || number >= korrode::persistence::kPatternsPerBank)
        return;
    const int bank = m_pendingBank >= 0 ? m_pendingBank : this->bank();
    m_pendingBank = -1;
    if (bank * korrode::persistence::kPatternsPerBank + number == m_bank.current)
        emit indexChanged(); // the bank stops blinking
    setPatternIndex(bank * korrode::persistence::kPatternsPerBank + number);
}

void PatternModel::setPatternIndex(int index)
{
    index = std::clamp(index, 0, korrode::persistence::kPatternCount - 1);
    if (index == m_bank.current)
        return;
    m_pendingBank = -1;
    beginResetModel();
    m_bank.current = index;
    endResetModel();
    sendCurrentPattern();
    emit indexChanged();
    emit lengthChanged();
    emit selectionChanged();
    scheduleSave();
}

void PatternModel::setLength(int length)
{
    length = std::clamp(length, 1, 16);
    if (length == current().length)
        return;
    current().length = std::uint8_t(length);

    Command c;
    c.kind = Command::Kind::SetLength;
    c.index = std::uint8_t(length);
    if (!m_engine.post(c))
        qCWarning(lcPattern) << "command queue full; length change not sent";

    emit dataChanged(index(0), index(rowCount() - 1), {ActiveRole});
    emit lengthChanged();
    scheduleSave();
}

void PatternModel::setSelectedStep(int step)
{
    step = std::clamp(step, 0, 15);
    if (step == m_selected)
        return;
    m_selected = step;
    emit selectedStepChanged();
    emit selectionChanged();
}

int PatternModel::selectedNote() const
{
    return selected().gate == Gate::Rest ? -1 : selected().note;
}

void PatternModel::setKeyboardMode(bool on)
{
    if (on == m_keyboardMode)
        return;
    m_keyboardMode = on;
    if (!on)
        auditionOff();
    emit modesChanged();
}

void PatternModel::setTimeMode(bool on)
{
    if (on == m_timeMode)
        return;
    m_timeMode = on;
    emit modesChanged();
}

template<typename Edit>
void PatternModel::editSelected(Edit edit)
{
    Step &s = current().steps[std::size_t(m_selected)];
    const Step before = s;
    edit(s);
    if (before == s)
        return;

    Command c;
    c.kind = Command::Kind::SetStep;
    c.index = std::uint8_t(m_selected);
    c.step = s;
    if (!m_engine.post(c))
        qCWarning(lcPattern) << "command queue full; step edit not sent";

    const QModelIndex row = index(m_selected);
    emit dataChanged(row, row);
    emit selectionChanged();
    scheduleSave();
}

void PatternModel::setNote(int semitone)
{
    editSelected([semitone](Step &s) {
        s.note = std::uint8_t(std::clamp(semitone, 0, 12));
        if (s.gate == Gate::Rest)
            s.gate = Gate::Note;
    });
}

void PatternModel::octaveDown()
{
    editSelected([](Step &s) { s.octave = std::int8_t(std::max(-1, s.octave - 1)); });
}

void PatternModel::octaveUp()
{
    editSelected([](Step &s) { s.octave = std::int8_t(std::min(1, s.octave + 1)); });
}

void PatternModel::toggleAccent()
{
    editSelected([](Step &s) { s.accent = !s.accent; });
}

void PatternModel::toggleSlide()
{
    editSelected([](Step &s) { s.slide = !s.slide; });
}

void PatternModel::setGate(int gate)
{
    editSelected([gate](Step &s) { s.gate = Gate(std::clamp(gate, 0, 2)); });
}

void PatternModel::selectNext()
{
    setSelectedStep((m_selected + 1) % current().length);
}

void PatternModel::selectPrevious()
{
    const int len = current().length;
    setSelectedStep((std::min(m_selected, len - 1) + len - 1) % len);
}

void PatternModel::clear()
{
    beginResetModel();
    for (auto &s : current().steps)
        s = Step{0, 0, false, false, Gate::Rest};
    endResetModel();
    sendCurrentPattern();
    emit selectionChanged();
    scheduleSave();
}

void PatternModel::randomize()
{
    // Minor pentatonic plus the octave: always sounds like an acid line.
    static constexpr std::uint8_t kScale[] = {0, 3, 5, 7, 10, 12};
    auto *rng = QRandomGenerator::global();
    const auto chance = [rng](double p) { return rng->generateDouble() < p; };

    // DENSITY: the chance a step plays (the first always does); a playing
    // step after another is sometimes a tie.
    const double density = m_engine.param(int(korrode::ParamId::Density));
    beginResetModel();
    bool previous = false;
    for (std::size_t i = 0; i < current().steps.size(); ++i) {
        Step &s = current().steps[i];
        const bool plays = i == 0 || chance(density);
        s.gate = !plays ? Gate::Rest : previous && chance(0.12) ? Gate::Tie : Gate::Note;
        previous = plays;
        s.note = kScale[rng->bounded(int(std::size(kScale)))];
        // Mostly the middle octave; the root note likes to jump.
        const double o = rng->generateDouble();
        s.octave = std::int8_t(o < 0.12 ? -1 : o < (s.note == 0 ? 0.45 : 0.25) ? 1 : 0);
        s.accent = chance(0.25);
        s.slide = chance(0.2);
    }
    endResetModel();
    sendCurrentPattern();
    emit selectionChanged();
    scheduleSave();
}

void PatternModel::auditionOn(int semitone)
{
    Command c;
    c.kind = Command::Kind::LiveNoteOn;
    c.index = std::uint8_t(kAuditionBaseNote + std::clamp(semitone, 0, 12));
    m_engine.post(c);
}

void PatternModel::auditionOff()
{
    Command c;
    c.kind = Command::Kind::LiveNoteOff;
    m_engine.post(c);
}

void PatternModel::save()
{
    m_saveTimer.stop();
    if (korrode::persistence::savePatterns(m_filePath, m_bank))
        m_dirty = false;
    else
        qCWarning(lcPattern) << "could not save patterns to" << m_filePath;
}

void PatternModel::sendCurrentPattern()
{
    Command c;
    c.kind = Command::Kind::LoadPattern;
    c.pattern = current();
    if (!m_engine.post(c))
        qCWarning(lcPattern) << "command queue full; pattern not sent";
}

void PatternModel::scheduleSave()
{
    m_dirty = true;
    m_saveTimer.start(); // restarts: saves 2 s after the last edit
}
