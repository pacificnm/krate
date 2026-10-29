#include "krimson/app/ProgramController.h"

#include <algorithm>
#include <cmath>

#include <QJSEngine>
#include <QJsonArray>
#include <QLoggingCategory>

#include "krimson/app/KrimsonController.h"
#include "krimson/app/SlotController.h"
#include "krimson/dsp/Engine.h"

Q_LOGGING_CATEGORY(lcPrograms, "krimson.programs")

using namespace krimson::persistence;

namespace {
constexpr int kSaveDelayMs = 2000;
constexpr qreal kSame = 1e-4;

QString keyOf(int index)
{
    const auto k = krimson::kParams[std::size_t(index)].key;
    return QString::fromUtf8(k.data(), qsizetype(k.size()));
}

void report(LoadResult result, const QString &path)
{
    if (result == LoadResult::Loaded)
        qCInfo(lcPrograms) << "loaded" << path;
    else if (result == LoadResult::Invalid)
        qCWarning(lcPrograms) << "could not read" << path << "- starting with the defaults";
}
} // namespace

ProgramController::ProgramController(KrimsonController &panel, SlotController &slotSetup, krimson::Engine &engine,
                                     const QString &programsPath, const QString &performancesPath, QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_slotSetup(slotSetup)
    , m_engine(engine)
    , m_programsPath(programsPath)
    , m_performancesPath(performancesPath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    const LoadResult programs = loadPrograms(m_programsPath, &m_bank);
    report(programs, m_programsPath);
    if (programs == LoadResult::NewerVersion)
        m_programsPath += QStringLiteral(".v1");
    const LoadResult performances = loadPerformances(m_performancesPath, &m_performances);
    report(performances, m_performancesPath);
    if (performances == LoadResult::NewerVersion)
        m_performancesPath += QStringLiteral(".v1");

    // The slots keep their own saved sound (slots.json); the programs only say
    // whether that still matches. A first run (neither file) starts each slot on
    // its program (A-D: 11-14).
    if (programs == LoadResult::Missing && !m_slotSetup.loadedFromFile())
        for (int slot = 0; slot < krimson::SlotCount; ++slot)
            selectForSlot(slot, m_bank.current[std::size_t(slot)]);
    updateEdited();
    connect(&m_panel, &krate::ParamController::paramsChanged, this, &ProgramController::updateEdited);
    connect(&m_panel, &KrimsonController::morphChanged, this, &ProgramController::updateEdited);
    connect(&m_panel, &KrimsonController::editedSlotChanged, this, &ProgramController::notify);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &ProgramController::save);
    m_pollTimer.setInterval(30);
    connect(&m_pollTimer, &QTimer::timeout, this, &ProgramController::poll);
    m_pollTimer.start();
}

ProgramController::~ProgramController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

ProgramController *ProgramController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int ProgramController::number() const
{
    return m_bank.current[std::size_t(m_engine.editedSlot())];
}

QString ProgramController::name() const
{
    return m_bank.programs[std::size_t(number())].name;
}

QString ProgramController::programName(int index) const
{
    return index >= 0 && index < kProgramCount ? m_bank.programs[std::size_t(index)].name : QString();
}

void ProgramController::save()
{
    m_saveTimer.stop();
    if (!savePrograms(m_programsPath, m_bank))
        qCWarning(lcPrograms) << "could not save" << m_programsPath;
}

void ProgramController::notify()
{
    updateEdited();
    ++m_revision;
    emit changed();
}

void ProgramController::poll()
{
    while (const auto pc = m_engine.popProgramChange())
        for (int slot = 0; slot < krimson::SlotCount; ++slot)
            if (pc->slotMask & (1u << slot))
                selectForSlot(slot, pc->program % kProgramCount);
}

void ProgramController::updateEdited()
{
    const int slot = m_engine.editedSlot();
    const Program &program = m_bank.programs[std::size_t(number())];
    bool edited = false;
    for (int i = 0; i < krimson::SlotParamCount && !edited; ++i) {
        const auto id = krimson::ParamId(i);
        edited = std::abs(m_engine.slotParam(slot, id) - program.values.value(keyOf(i))) > kSame;
        if (krimson::Engine::morphable(id)) {
            const qreal target = program.morph.value(keyOf(i), -1.0);
            edited = edited || std::abs(m_engine.morph(slot, id) - target) > kSame;
        }
    }
    if (edited != m_edited) {
        m_edited = edited;
        ++m_revision;
        emit changed();
    }
}

void ProgramController::selectForSlot(int slot, int index)
{
    if (slot < 0 || slot >= krimson::SlotCount || index < 0 || index >= kProgramCount)
        return;
    const Program &program = m_bank.programs[std::size_t(index)];
    for (int i = 0; i < krimson::SlotParamCount; ++i)
        if (const auto it = program.values.find(keyOf(i)); it != program.values.end())
            m_engine.setSlotParam(slot, krimson::ParamId(i), float(it.value()));
    m_engine.clearMorph(slot);
    for (auto it = program.morph.begin(); it != program.morph.end(); ++it)
        for (int i = 0; i < krimson::SlotParamCount; ++i)
            if (keyOf(i) == it.key())
                m_engine.setMorph(slot, krimson::ParamId(i), float(it.value()));
    m_bank.current[std::size_t(slot)] = index;
    m_engine.markAllChanged(); // the panel follows (and slots.json saves the new sound)
    m_panel.morphReloaded();
    m_storing = false;
    m_saveTimer.start();
    notify();
}

void ProgramController::select(int index)
{
    selectForSlot(m_engine.editedSlot(), index);
}

void ProgramController::step(int delta)
{
    if (m_storing) {
        m_storeTarget = ((m_storeTarget + delta) % kProgramCount + kProgramCount) % kProgramCount;
        notify();
        return;
    }
    select(((number() + delta) % kProgramCount + kProgramCount) % kProgramCount);
}

void ProgramController::pressStore()
{
    if (!m_storing) {
        m_storing = true;
        m_storeTarget = number();
        notify();
        return;
    }
    // Write the edited slot's sound into the destination.
    const int slot = m_engine.editedSlot();
    Program &program = m_bank.programs[std::size_t(m_storeTarget)];
    for (int i = 0; i < krimson::SlotParamCount; ++i)
        program.values[keyOf(i)] = m_engine.slotParam(slot, krimson::ParamId(i));
    program.morph.clear();
    for (int i = 0; i < krimson::SlotParamCount; ++i)
        if (const float target = m_engine.morph(slot, krimson::ParamId(i)); target >= 0.0f)
            program.morph.insert(keyOf(i), target);
    if (program.name == initProgram().name)
        program.name = QStringLiteral("Program %1%2").arg(m_storeTarget / kProgramsPerBank + 1).arg(m_storeTarget % kProgramsPerBank + 1);
    m_bank.current[std::size_t(slot)] = m_storeTarget;
    m_storing = false;
    save();
    notify();
    qCInfo(lcPrograms) << "stored" << program.name << "at" << m_storeTarget;
}

void ProgramController::cancelStore()
{
    if (!m_storing)
        return;
    m_storing = false;
    notify();
}

bool ProgramController::performanceUsed(int index) const
{
    return index >= 0 && index < kPerformanceCount && !m_performances.performances[std::size_t(index)].state.isEmpty();
}

void ProgramController::stepPerformance(int delta)
{
    m_performances.current = ((m_performances.current + delta) % kPerformanceCount + kPerformanceCount) % kPerformanceCount;
    notify();
}

void ProgramController::loadPerformance(int index)
{
    if (!performanceUsed(index))
        return;
    m_performances.current = index;
    const QJsonObject state = m_performances.performances[std::size_t(index)].state;
    m_slotSetup.fromJson(state);
    // Each slot's program number comes with the performance.
    const QJsonArray programs = state.value(u"programs").toArray();
    for (int slot = 0; slot < krimson::SlotCount && slot < programs.size(); ++slot)
        m_bank.current[std::size_t(slot)] = std::clamp(programs.at(slot).toInt(0), 0, kProgramCount - 1);
    m_slotSetup.save();
    m_saveTimer.start();
    notify();
}

void ProgramController::savePerformance(int index)
{
    if (index < 0 || index >= kPerformanceCount)
        return;
    QJsonObject state = m_slotSetup.toJson();
    QJsonArray programs;
    for (int c : m_bank.current)
        programs.append(c);
    state.insert(QStringLiteral("programs"), programs);
    Performance &p = m_performances.performances[std::size_t(index)];
    p.state = state;
    if (p.name.isEmpty())
        p.name = QStringLiteral("Performance %1").arg(index + 1);
    m_performances.current = index;
    if (!savePerformances(m_performancesPath, m_performances))
        qCWarning(lcPrograms) << "could not save" << m_performancesPath;
    notify();
}
