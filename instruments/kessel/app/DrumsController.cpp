#include "kessel/app/DrumsController.h"

#include "app/RunControl.h"

#include <algorithm>
#include <array>

#include <QJSEngine>
#include <QLoggingCategory>

#include "core/Transport.h"
#include "kessel/dsp/Engine.h"
#include "kessel/dsp/Params.h"

Q_LOGGING_CATEGORY(lcPatterns, "kessel.patterns")

namespace {
constexpr int kSaveDelayMs = 2000;
constexpr std::array<int, 4> kScaleSteps{4, 8, 3, 6}; // in scaleNames() order
} // namespace

DrumsController::DrumsController(kessel::Engine &engine, const QString &panelFilePath,
                                 const QString &patternsFilePath, krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
    , m_patternsFilePath(patternsFilePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    using kessel::persistence::LoadResult;
    switch (kessel::persistence::loadPatterns(m_patternsFilePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcPatterns) << "loaded patterns from" << m_patternsFilePath;
        break;
    case LoadResult::Missing:
        break; // first run: the factory patterns
    case LoadResult::Invalid:
        qCWarning(lcPatterns) << "could not read" << m_patternsFilePath << "- starting with the factory patterns";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcPatterns) << m_patternsFilePath << "was written by a newer version; saving to .v1 instead";
        m_patternsFilePath += QStringLiteral(".v1");
        break;
    }
    m_engine.setPattern(current(), m_bank.current); // before processing starts
    m_playingPattern = m_bank.current;

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &DrumsController::save);

    m_pollTimer.setInterval(16); // ~60 Hz
    connect(&m_pollTimer, &QTimer::timeout, this, &DrumsController::poll);
    m_pollTimer.start();

    syncTempo(); // the saved Tempo knob position
    connect(this, &ParamController::paramsChanged, this, &DrumsController::syncTempo);
}

DrumsController::~DrumsController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

DrumsController *DrumsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DrumsController::save()
{
    m_saveTimer.stop();
    if (!kessel::persistence::savePatterns(m_patternsFilePath, m_bank))
        qCWarning(lcPatterns) << "could not save patterns to" << m_patternsFilePath;
}

bool DrumsController::playing() const
{
    if (m_transport)
        return m_transport->playing();
    return m_run && m_run->running(m_runId);
}

void DrumsController::setPlaying(bool playing)
{
    if (!m_transport && m_run) {
        m_run->setRunning(m_runId, playing);
        return;
    }
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
    m_playing = playing;
    emit playingChanged();
}

void DrumsController::poll()
{
    const int step = m_engine.currentStep();
    if (step != m_currentStep) {
        m_currentStep = step;
        emit currentStepChanged();
    }
    const int playingPattern = m_engine.playingSlot();
    const int queuedPattern = m_engine.queuedSlot();
    if (playingPattern != m_playingPattern || queuedPattern != m_queuedPattern) {
        m_playingPattern = playingPattern;
        m_queuedPattern = queuedPattern;
        emit playingPatternChanged();
    }
    // The transport can also be started by a MIDI button, Space, or the rack.
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void DrumsController::setSelectedTrack(int track)
{
    track = std::clamp(track, 0, kessel::DrumCount - 1);
    if (track == m_selectedTrack)
        return;
    m_selectedTrack = track;
    emit selectedTrackChanged();
}

void DrumsController::setPatternIndex(int index)
{
    index = std::clamp(index, 0, kessel::persistence::kPatternCount - 1);
    if (index == m_bank.current)
        return;
    m_bank.current = index;
    m_engine.postPattern(current(), index);
    edited();
}

void DrumsController::post(kessel::Command &command)
{
    command.slot = std::uint8_t(m_bank.current);
    m_engine.post(command);
}

void DrumsController::edited()
{
    ++m_patternRevision;
    emit patternChanged();
    m_saveTimer.start(); // restarts: saves 2 s after the last edit
}

void DrumsController::setLength(int length)
{
    length = std::clamp(length, 1, 16);
    if (length == current().length)
        return;
    current().length = std::uint8_t(length);
    kessel::Command command;
    command.kind = kessel::Command::Kind::SetLength;
    command.value = std::uint8_t(length);
    post(command);
    edited();
}

int DrumsController::scaleIndex() const
{
    const auto it = std::find(kScaleSteps.begin(), kScaleSteps.end(), int(current().stepsPerBeat));
    return it == kScaleSteps.end() ? 0 : int(it - kScaleSteps.begin());
}

void DrumsController::setScaleIndex(int index)
{
    index = std::clamp(index, 0, int(kScaleSteps.size()) - 1);
    if (index == scaleIndex())
        return;
    current().stepsPerBeat = std::uint8_t(kScaleSteps[std::size_t(index)]);
    kessel::Command command;
    command.kind = kessel::Command::Kind::SetStepsPerBeat;
    command.value = current().stepsPerBeat;
    post(command);
    edited();
}

void DrumsController::setOrderIndex(int index)
{
    index = std::clamp(index, 0, 3);
    if (index == current().order)
        return;
    current().order = std::uint8_t(index);
    kessel::Command command;
    command.kind = kessel::Command::Kind::SetOrder;
    command.value = std::uint8_t(index);
    post(command);
    edited();
}

QStringList DrumsController::scaleNames() const
{
    return {QStringLiteral("1/16"), QStringLiteral("1/32"), QStringLiteral("1/8 triplet"),
            QStringLiteral("1/16 triplet")};
}

QStringList DrumsController::orderNames() const
{
    return {QStringLiteral("Forward"), QStringLiteral("Reverse"), QStringLiteral("Fwd+Rev"), QStringLiteral("Random")};
}

int DrumsController::stepState(int track, int step) const
{
    if (track < 0 || track >= kessel::DrumCount || step < 0 || step >= 16)
        return 0;
    return int(current().tracks[std::size_t(track)][std::size_t(step)]);
}

void DrumsController::setStepState(int track, int step, int state)
{
    if (track < 0 || track >= kessel::DrumCount || step < 0 || step >= 16 || state < 0 || state > 3)
        return;
    auto &slot = current().tracks[std::size_t(track)][std::size_t(step)];
    if (int(slot) == state)
        return;
    slot = kessel::StepState(state);
    kessel::Command command;
    command.kind = kessel::Command::Kind::SetStep;
    command.drum = std::uint8_t(track);
    command.step = std::uint8_t(step);
    command.state = slot;
    post(command);
    edited();
}

void DrumsController::cycleStep(int step)
{
    setStepState(m_selectedTrack, step, (stepState(m_selectedTrack, step) + 1) % 4);
}

namespace {
bool validStep(int track, int step) { return track >= 0 && track < kessel::DrumCount && step >= 0 && step < 16; }
// Each track's TUNE and DECAY parameter keys (DrumId order); "" = none.
constexpr std::array<const char *, kessel::DrumCount> kTuneKeys{
    "bd_tune", "sd_tune", "lt_tune", "mt_tune", "ht_tune", "rs_tune", "cp_tune", "hh_tune",
    "hh_tune", "cr_tune", "rd_tune", "me_tune", "ck_tune", "nz_tone", "bm_tune", "sm_tune"};
constexpr std::array<const char *, kessel::DrumCount> kDecayKeys{
    "bd_decay", "sd_decay", "lt_decay", "mt_decay", "ht_decay", "rs_decay", "cp_decay", "ch_decay",
    "oh_decay", "", "", "me_decay", "ck_decay", "nz_decay", "bm_decay", "sm_decay"};
} // namespace

QString DrumsController::tuneKey(int track) const
{
    return track >= 0 && track < kessel::DrumCount ? QString::fromLatin1(kTuneKeys[std::size_t(track)]) : QString();
}

QString DrumsController::decayKey(int track) const
{
    return track >= 0 && track < kessel::DrumCount ? QString::fromLatin1(kDecayKeys[std::size_t(track)]) : QString();
}

int DrumsController::stepVelocity(int track, int step) const
{
    return validStep(track, step) ? current().velocity[std::size_t(track)][std::size_t(step)] : 0;
}

qreal DrumsController::stepTune(int track, int step) const
{
    if (!validStep(track, step))
        return -1.0;
    const std::uint8_t lock = current().tune[std::size_t(track)][std::size_t(step)];
    return lock > 0 ? qreal(kessel::lockValue(lock)) : -1.0;
}

qreal DrumsController::stepDecay(int track, int step) const
{
    if (!validStep(track, step))
        return -1.0;
    const std::uint8_t lock = current().decay[std::size_t(track)][std::size_t(step)];
    return lock > 0 ? qreal(kessel::lockValue(lock)) : -1.0;
}

bool DrumsController::stepHasDetails(int track, int step) const
{
    return stepVelocity(track, step) > 0 || stepTune(track, step) >= 0.0 || stepDecay(track, step) >= 0.0;
}

void DrumsController::setStepVelocity(int track, int step, int velocity)
{
    if (!validStep(track, step))
        return;
    current().velocity[std::size_t(track)][std::size_t(step)] = std::uint8_t(std::clamp(velocity, 0, 127));
    postDetail(track, step);
}

void DrumsController::setStepTune(int track, int step, qreal value)
{
    if (!validStep(track, step))
        return;
    current().tune[std::size_t(track)][std::size_t(step)] = value < 0.0 ? 0 : kessel::lockOf(value);
    postDetail(track, step);
}

void DrumsController::setStepDecay(int track, int step, qreal value)
{
    if (!validStep(track, step))
        return;
    current().decay[std::size_t(track)][std::size_t(step)] = value < 0.0 ? 0 : kessel::lockOf(value);
    postDetail(track, step);
}

void DrumsController::clearStepDetails(int track, int step)
{
    if (!validStep(track, step))
        return;
    const auto t = std::size_t(track), s = std::size_t(step);
    current().velocity[t][s] = current().tune[t][s] = current().decay[t][s] = 0;
    postDetail(track, step);
}

void DrumsController::postDetail(int track, int step)
{
    const auto t = std::size_t(track), s = std::size_t(step);
    kessel::Command command;
    command.kind = kessel::Command::Kind::SetStepDetail;
    command.drum = std::uint8_t(track);
    command.step = std::uint8_t(step);
    command.value = current().velocity[t][s];
    command.tune = current().tune[t][s];
    command.decay = current().decay[t][s];
    post(command);
    edited();
}

void DrumsController::clearTrack(int track)
{
    if (track < 0 || track >= kessel::DrumCount)
        return;
    current().tracks[std::size_t(track)].fill(kessel::StepState::Off);
    current().velocity[std::size_t(track)].fill(0);
    current().tune[std::size_t(track)].fill(0);
    current().decay[std::size_t(track)].fill(0);
    m_engine.postReplace(current(), m_bank.current);
    edited();
}

void DrumsController::clearPattern()
{
    const std::uint8_t length = current().length, scale = current().stepsPerBeat, order = current().order;
    current() = kessel::Pattern{};
    current().length = length;
    current().stepsPerBeat = scale;
    current().order = order;
    m_engine.postReplace(current(), m_bank.current);
    edited();
}

void DrumsController::audition(int track)
{
    if (track < 0 || track >= kessel::DrumCount)
        return;
    kessel::Command command;
    command.kind = kessel::Command::Kind::Trigger;
    command.drum = std::uint8_t(track);
    command.velocity = 0.85f; // a normal (unaccented) sequencer step
    m_engine.post(command);
}

void DrumsController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(kessel::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}

void DrumsController::setRunControl(krate::RunControl *run, const QString &id)
{
    m_run = run;
    m_runId = id;
    if (!m_run)
        return;
    m_run->addRunner(id);
    connect(m_run, &krate::RunControl::changed, this, &DrumsController::playingChanged);
}
