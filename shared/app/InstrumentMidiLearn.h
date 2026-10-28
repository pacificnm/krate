#pragma once

#include <span>

#include <QStringList>

#include "app/MidiLearnController.h"
#include "app/MidiMapFile.h"
#include "core/ParamDescriptor.h"

namespace krate {
class IInstrument;
class MidiLearnMap;
}

namespace krate {

// MIDI Learn for a standalone app: one instrument's krate::MidiLearnMap, by key
// (parameter keys and the app's action keys), saved to midimap.json. The
// binding itself happens on the audio thread; this mirrors and saves it.
class InstrumentMidiLearn final : public MidiLearnController
{
    Q_OBJECT

public:
    // The instrument must provide a MidiLearnMap.
    InstrumentMidiLearn(IInstrument &instrument, const QString &filePath, QObject *parent,
                        const QStringList &actions = {});
    ~InstrumentMidiLearn() override;

    QString inputPortMatch() const { return m_map.inputPortMatch; }

    QString mappingFor(const QString &key) const override;
    void clear(const QString &key) override;
    void clearAll() override;
    void drainEvents() override;

protected:
    bool armTarget(const QString &key) override;

private:
    int indexOf(const QString &key) const; // parameter index or action target; kUnmapped if unknown
    QString keyOf(int target) const;
    void save();

    MidiLearnMap &m_learnMap;
    std::span<const ParamDescriptor> m_params;
    QStringList m_actions;
    QString m_filePath;
    files::MidiMap m_map;
};

} // namespace krate
