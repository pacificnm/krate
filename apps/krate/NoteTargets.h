#pragma once

#include <vector>

#include <QList>
#include <QString>

#include "app/InstrumentRegistry.h"

// The rack's instruments that play notes, as targets for the song player's
// tracks and the sequencer's: one per instrument, one per channel for
// instruments with several (Krimson's slots A-D). The metronome and the amp
// play none.
struct NoteTarget
{
    QString label; // "Krimson B"
    QString key;   // "krimson:B" (stable, for saved files)
    int device = -1;
    int channel = 0;
    bool drumMap = false; // Kessel: a song's drum notes are mapped to its voices
};

std::vector<NoteTarget> noteTargets(const QList<krate::InstrumentEntry> &entries);
