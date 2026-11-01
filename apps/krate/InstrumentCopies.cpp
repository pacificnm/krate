#include "InstrumentCopies.h"

#include <algorithm>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

QList<krate::InstrumentEntry> copyEntries(const krate::InstrumentEntry &base)
{
    QList<krate::InstrumentEntry> copies;
    for (int n = 2; n <= base.maxCopies; ++n) {
        krate::InstrumentEntry entry = base;
        entry.id = base.id + QString::number(n);
        entry.name = base.name + u' ' + QString::number(n);
        entry.kind = base.id;
        entry.copy = n;
        entry.inRackByDefault = false; // in the rack once added from +
        entry.maxCopies = 1;
        // Its MIDI channels stay in the entry (a song or the sequencer playing
        // it sends on them); the rack's channel routing is the first one's.
        copies.append(entry);
    }
    return copies;
}

void seedFolder(const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty() || QFileInfo::exists(to) || !QFileInfo(from).isDir())
        return;
    const QDir source(from);
    QDir().mkpath(to);
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        const QString target = QDir(to).filePath(source.relativeFilePath(info.filePath()));
        if (info.isDir())
            QDir().mkpath(target);
        else
            QFile::copy(info.filePath(), target);
    }
}

// --- CopyInstrument ----------------------------------------------------------------------

void CopyInstrument::setTarget(krate::IInstrument *target) noexcept
{
    if (target) { // as the rack has had it so far
        target->setSequencerEnabled(m_enabled.load(std::memory_order_relaxed));
        target->setSequencerSuspended(m_suspended.load(std::memory_order_relaxed));
    }
    m_target.store(target, std::memory_order_release);
}

void CopyInstrument::prepare(double sampleRate, std::uint32_t maxFrames) noexcept
{
    if (krate::IInstrument *t = target())
        t->prepare(sampleRate, maxFrames);
}

std::span<const krate::ParamDescriptor> CopyInstrument::parameters() const noexcept
{
    const krate::IInstrument *t = target();
    return t ? t->parameters() : m_first.parameters();
}

int CopyInstrument::outputChannels() const noexcept
{
    const krate::IInstrument *t = target();
    return t ? t->outputChannels() : m_first.outputChannels();
}

int CopyInstrument::inputChannels() const noexcept
{
    const krate::IInstrument *t = target();
    return t ? t->inputChannels() : m_first.inputChannels();
}

bool CopyInstrument::stereoInput() const noexcept
{
    const krate::IInstrument *t = target();
    return t ? t->stereoInput() : m_first.stereoInput();
}

float CopyInstrument::param(int index) const noexcept
{
    if (const krate::IInstrument *t = target())
        return t->param(index);
    const auto params = m_first.parameters();
    return index >= 0 && index < int(params.size()) ? params[std::size_t(index)].defaultValue : 0.0f;
}

void CopyInstrument::setParam(int index, float normalized) noexcept
{
    if (krate::IInstrument *t = target())
        t->setParam(index, normalized);
}

void CopyInstrument::setParamFromHost(int index, float normalized) noexcept
{
    if (krate::IInstrument *t = target())
        t->setParamFromHost(index, normalized);
}

std::uint64_t CopyInstrument::takeParamsChangedByAudio() noexcept
{
    krate::IInstrument *t = target();
    return t ? t->takeParamsChangedByAudio() : 0;
}

krate::MidiLearnMap *CopyInstrument::midiLearnMap() noexcept
{
    krate::IInstrument *t = target();
    return t ? t->midiLearnMap() : nullptr;
}

void CopyInstrument::setSequencerEnabled(bool on) noexcept
{
    m_enabled.store(on, std::memory_order_relaxed);
    if (krate::IInstrument *t = target())
        t->setSequencerEnabled(on);
}

void CopyInstrument::setSequencerSuspended(bool on) noexcept
{
    m_suspended.store(on, std::memory_order_relaxed);
    if (krate::IInstrument *t = target())
        t->setSequencerSuspended(on);
}

void CopyInstrument::process(const krate::AudioBlock &block) noexcept
{
    if (krate::IInstrument *t = target()) {
        t->process(block);
        return;
    }
    for (int c = 0; c < outputChannels(); ++c)
        std::fill_n(block.outputs[c], block.frames, 0.0f);
}

// --- CopyInstance ------------------------------------------------------------------------

CopyInstance::CopyInstance(const krate::InstrumentEntry &entry, krate::InstrumentContext context,
                           krate::InstrumentInstance &first, QString firstConfig, QString firstData)
    : m_create(entry.create)
    , m_context(std::move(context))
    , m_firstConfig(std::move(firstConfig))
    , m_firstData(std::move(firstData))
    , m_device(first.instrument())
{
}

CopyInstance::~CopyInstance()
{
    m_device.setTarget(nullptr); // before the instrument goes
}

void CopyInstance::build()
{
    if (m_instance)
        return;
    seedFolder(m_firstConfig, m_context.configDir);
    seedFolder(m_firstData, m_context.dataDir);
    m_instance = m_create(m_context);
    m_device.setTarget(&m_instance->instrument());
}

void CopyInstance::triggerAction(const QString &action)
{
    if (m_instance)
        m_instance->triggerAction(action);
}

QVariantMap CopyInstance::panelProperties()
{
    return m_instance ? m_instance->panelProperties() : QVariantMap{};
}

QJsonObject CopyInstance::saveSound()
{
    return m_instance ? m_instance->saveSound() : QJsonObject{};
}

void CopyInstance::loadSound(const QJsonObject &sound)
{
    if (sound.isEmpty())
        return;
    build();
    m_instance->loadSound(sound);
}

QStringList CopyInstance::sampleTargets() const
{
    return m_instance ? m_instance->sampleTargets() : QStringList{};
}

QStringList CopyInstance::sampleTargetContents() const
{
    return m_instance ? m_instance->sampleTargetContents() : QStringList{};
}

bool CopyInstance::assignSample(int target, const QString &path, QString *error)
{
    build();
    return m_instance->assignSample(target, path, error);
}

void CopyInstance::clearSample(int target)
{
    if (m_instance)
        m_instance->clearSample(target);
}
