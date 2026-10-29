#include "BackingInstrument.h"

krate::InstrumentEntry backingEntry(const krate::song::SongPlayer &player)
{
    krate::InstrumentEntry entry;
    entry.id = QStringLiteral("backing");
    entry.name = QStringLiteral("Backing");
    entry.playsNotes = false;
    entry.create = [&player](const krate::InstrumentContext &) -> std::unique_ptr<krate::InstrumentInstance> {
        return std::make_unique<BackingInstance>(player);
    };
    return entry;
}
