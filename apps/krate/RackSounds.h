#pragma once

#include <vector>

#include <QJsonObject>
#include <QStringList>

class RackController;
namespace krate {
class InstrumentInstance;
}

// Every instrument's sound (all its knobs; Krimson's slots too) and the
// mixer, taken and put back together: for songs and saved sequences that
// bring their own sounds. GUI thread.
class RackSounds
{
public:
    struct Snapshot
    {
        QJsonObject sounds; // instrument id -> its sound
        QJsonObject mixer;
    };

    // `instances` in the order of `ids` (null entries are skipped).
    RackSounds(RackController &rack, QStringList ids, std::vector<krate::InstrumentInstance *> instances);

    Snapshot capture() const;
    // The instruments it has sounds for, and the mixer if it has one.
    void restore(const Snapshot &snapshot) const;

private:
    RackController &m_rack;
    QStringList m_ids;
    std::vector<krate::InstrumentInstance *> m_instances;
};
