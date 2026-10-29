#pragma once

#include "app/InstrumentRegistry.h"
#include "song/BackingTrack.h"

// The song's backing track as a rack device (docs/SONG_PLAYER.md#backing-track):
// a mixer strip ("Backing": its fader, pan, mute and solo, kept with songs'
// mixers) and no panel; the Song screen loads and syncs it.
class BackingInstance final : public krate::InstrumentInstance
{
public:
    explicit BackingInstance(const krate::song::SongPlayer &player) : track(player) {}
    krate::IInstrument &instrument() override { return track; }
    // Nothing of its own to keep with a song's sounds (its level is the mixer's).
    QJsonObject saveSound() override { return {}; }
    void loadSound(const QJsonObject &) override {}

    krate::song::BackingTrack track;
};

// Its entry: no panel, no notes, no MIDI channel.
krate::InstrumentEntry backingEntry(const krate::song::SongPlayer &player);
