#include "NoteTargets.h"

#include <algorithm>

std::vector<NoteTarget> noteTargets(const QList<krate::InstrumentEntry> &entries)
{
    std::vector<NoteTarget> targets;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const krate::InstrumentEntry &e = entries[i];
        if (!e.playsNotes)
            continue;
        if (e.extraMidiChannels.isEmpty()) {
            targets.push_back({e.name, e.id, int(i), std::max(0, e.defaultMidiChannel), e.id == u"kessel"});
            continue;
        }
        QList<int> channels{e.defaultMidiChannel};
        channels += e.extraMidiChannels;
        for (qsizetype k = 0; k < channels.size(); ++k) {
            const QChar slot(char16_t(u'A' + k));
            targets.push_back({e.name + u' ' + slot, e.id + u':' + slot, int(i), std::max(0, channels[k]), false});
        }
    }
    return targets;
}
