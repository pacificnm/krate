// song-dump: lists what is in a .gp tab file and/or a .mid file, to check how
// they line up before the real readers are written (docs/SONG_PLAYER.md, S0).
//
//   song-dump [--bars N] [--track T] [--xml OUT] FILE...
//   song-dump --compare FILE.gp FILE.mid   (Krate's tab reader against the MIDI file)
//
// For a .gp file: the zip's entries, the score's tracks (name, instrument,
// tuning, capo), bar count, time signatures, repeats, tempo changes, and the
// first N bars of track T as string/fret. --xml writes Content/score.gpif out.
// For a .mid file: format, division, tempo map, and per track its name,
// channels, note count, note range and first notes. Lyrics are never printed.

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QXmlStreamReader>

#include <zlib.h>

#include "song/GpFile.h"
#include "song/MidiFile.h"

#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace {

// --- zip ----------------------------------------------------------------------

std::uint32_t le32(const QByteArray &b, qsizetype at)
{
    if (at < 0 || at + 4 > b.size())
        return 0;
    return std::uint32_t(std::uint8_t(b[at])) | std::uint32_t(std::uint8_t(b[at + 1])) << 8
           | std::uint32_t(std::uint8_t(b[at + 2])) << 16 | std::uint32_t(std::uint8_t(b[at + 3])) << 24;
}
std::uint16_t le16(const QByteArray &b, qsizetype at)
{
    if (at < 0 || at + 2 > b.size())
        return 0;
    return std::uint16_t(std::uint8_t(b[at]) | std::uint8_t(b[at + 1]) << 8);
}

struct ZipEntry
{
    QString name;
    int method = 0;
    std::uint32_t compressed = 0, size = 0, localOffset = 0;
};

std::vector<ZipEntry> zipEntries(const QByteArray &zip)
{
    std::vector<ZipEntry> out;
    qsizetype eocd = -1;
    for (qsizetype i = zip.size() - 22; i >= 0 && i >= zip.size() - 22 - 65535; --i)
        if (le32(zip, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd < 0)
        return out;
    const int count = le16(zip, eocd + 10);
    qsizetype p = le32(zip, eocd + 16);
    for (int i = 0; i < count && le32(zip, p) == 0x02014b50; ++i) {
        ZipEntry e;
        e.method = le16(zip, p + 10);
        e.compressed = le32(zip, p + 20);
        e.size = le32(zip, p + 24);
        const int nameLen = le16(zip, p + 28), extraLen = le16(zip, p + 30), commentLen = le16(zip, p + 32);
        e.localOffset = le32(zip, p + 42);
        e.name = QString::fromUtf8(zip.mid(p + 46, nameLen));
        out.push_back(e);
        p += 46 + nameLen + extraLen + commentLen;
    }
    return out;
}

QByteArray zipRead(const QByteArray &zip, const ZipEntry &e)
{
    const qsizetype local = e.localOffset;
    if (le32(zip, local) != 0x04034b50)
        return {};
    const qsizetype data = local + 30 + le16(zip, local + 26) + le16(zip, local + 28);
    const QByteArray raw = zip.mid(data, e.compressed);
    if (e.method == 0)
        return raw;
    if (e.method != 8)
        return {};
    QByteArray out(qsizetype(e.size), Qt::Uninitialized);
    z_stream z{};
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
        return {};
    z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.data()));
    z.avail_in = uInt(raw.size());
    z.next_out = reinterpret_cast<Bytef *>(out.data());
    z.avail_out = uInt(out.size());
    const int r = inflate(&z, Z_FINISH);
    inflateEnd(&z);
    return r == Z_STREAM_END ? out : QByteArray();
}

// --- a small XML tree ---------------------------------------------------------

struct Node
{
    QString name;
    QHash<QString, QString> attrs;
    QString text;
    std::vector<std::unique_ptr<Node>> children;

    const Node *child(QStringView n) const
    {
        for (const auto &c : children)
            if (c->name == n)
                return c.get();
        return nullptr;
    }
    QString childText(QStringView n) const
    {
        const Node *c = child(n);
        return c ? c->text.trimmed() : QString();
    }
    // A <Property name="..."> under <Properties>.
    const Node *property(QStringView n) const
    {
        if (const Node *props = child(u"Properties"))
            for (const auto &p : props->children)
                if (p->attrs.value(QStringLiteral("name")) == n)
                    return p.get();
        return nullptr;
    }
};

std::unique_ptr<Node> parseXml(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    auto root = std::make_unique<Node>();
    std::vector<Node *> stack{root.get()};
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            auto n = std::make_unique<Node>();
            n->name = r.name().toString();
            for (const auto &a : r.attributes())
                n->attrs.insert(a.name().toString(), a.value().toString());
            Node *raw = n.get();
            stack.back()->children.push_back(std::move(n));
            stack.push_back(raw);
        } else if (r.isEndElement()) {
            stack.pop_back();
        } else if (r.isCharacters() && stack.size() > 1) {
            stack.back()->text += r.text();
        }
    }
    if (r.hasError())
        std::fprintf(stderr, "XML error: %s\n", qPrintable(r.errorString()));
    return root;
}

// Index children of <Collection> by their id attribute.
QHash<QString, const Node *> byId(const Node *collection)
{
    QHash<QString, const Node *> out;
    if (collection)
        for (const auto &c : collection->children)
            out.insert(c->attrs.value(QStringLiteral("id")), c.get());
    return out;
}

const char *noteName(int midi)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[((midi % 12) + 12) % 12];
}

void dumpGp(const QString &path, int bars, int track, const QString &xmlOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "can't open %s\n", qPrintable(path));
        return;
    }
    const QByteArray zip = f.readAll();
    std::printf("== %s (%lld bytes)\n", qPrintable(path), static_cast<long long>(zip.size()));
    const auto entries = zipEntries(zip);
    if (entries.empty()) {
        std::printf("not a zip archive (an older binary tab format?) - first bytes: %s\n",
                    zip.left(16).toHex(' ').constData());
        return;
    }
    QByteArray gpif;
    for (const auto &e : entries) {
        std::printf("  zip: %-40s method %d  %u -> %u bytes\n", qPrintable(e.name), e.method, e.compressed, e.size);
        if (e.name.endsWith(u"score.gpif"))
            gpif = zipRead(zip, e);
    }
    if (gpif.isEmpty()) {
        std::printf("no score.gpif found (or it failed to inflate)\n");
        return;
    }
    if (!xmlOut.isEmpty()) {
        QFile out(xmlOut);
        if (out.open(QIODevice::WriteOnly))
            out.write(gpif);
        std::printf("  wrote %s\n", qPrintable(xmlOut));
    }
    const auto doc = parseXml(gpif);
    const Node *gp = doc->child(u"GPIF");
    if (!gp) {
        std::printf("no <GPIF> root\n");
        return;
    }
    std::printf("\nversion %s / %s\n", qPrintable(gp->childText(u"GPVersion")),
                qPrintable(gp->child(u"GPRevision") ? gp->child(u"GPRevision")->text.trimmed() : QString()));
    std::printf("top-level elements:");
    for (const auto &c : gp->children)
        std::printf(" %s(%zu)", qPrintable(c->name), c->children.size());
    std::printf("\n");
    if (const Node *score = gp->child(u"Score"))
        std::printf("title: %s | artist: %s | album: %s\n", qPrintable(score->childText(u"Title")),
                    qPrintable(score->childText(u"Artist")), qPrintable(score->childText(u"Album")));

    // Tracks.
    std::printf("\nTRACKS\n");
    int index = 0;
    if (const Node *tracks = gp->child(u"Tracks"))
        for (const auto &t : tracks->children) {
            QString tuning, capo;
            // Tuning lives on the staff (newer files) or the track (older).
            std::vector<const Node *> places{t.get()};
            if (const Node *staves = t->child(u"Staves"))
                for (const auto &s : staves->children)
                    places.push_back(s.get());
            for (const Node *n : places) {
                if (const Node *tp = n->property(u"Tuning"))
                    tuning = tp->childText(u"Pitches");
                if (const Node *cp = n->property(u"CapoFret"))
                    capo = cp->childText(u"Fret");
            }
            QString tuningNames;
            for (const QString &p : tuning.split(u' ', Qt::SkipEmptyParts))
                tuningNames += QString::fromLatin1(noteName(p.toInt())) + QString::number(p.toInt() / 12 - 1) + u' ';
            QString program, channel;
            if (const Node *midi = t->child(u"MidiConnection"))
                channel = midi->childText(u"PrimaryChannel");
            if (const Node *sounds = t->child(u"Sounds"))
                if (!sounds->children.empty())
                    if (const Node *m = sounds->children.front()->child(u"MIDI"))
                        program = m->childText(u"Program");
            const Node *instr = t->child(u"InstrumentSet");
            std::printf("  [%d] id %s  \"%s\"  instrument: %s (%s)  channel %s  program %s\n", index++,
                        qPrintable(t->attrs.value(QStringLiteral("id"))), qPrintable(t->childText(u"Name")),
                        qPrintable(instr ? instr->childText(u"Name") : QString()),
                        qPrintable(instr ? instr->childText(u"Type") : QString()), qPrintable(channel),
                        qPrintable(program));
            if (!tuning.isEmpty())
                std::printf("      tuning (MIDI): %s = %s capo %s\n", qPrintable(tuning), qPrintable(tuningNames),
                            qPrintable(capo.isEmpty() ? QStringLiteral("0") : capo));
        }

    // Master bars, time signatures, repeats.
    const Node *masterBars = gp->child(u"MasterBars");
    std::printf("\nMASTER BARS: %zu\n", masterBars ? masterBars->children.size() : std::size_t(0));
    QString lastTime;
    int bar = 0;
    if (masterBars)
        for (const auto &mb : masterBars->children) {
            ++bar;
            const QString time = mb->childText(u"Time");
            QString extra;
            if (const Node *rep = mb->child(u"Repeat"))
                extra += QStringLiteral(" repeat(start=%1 end=%2 count=%3)")
                             .arg(rep->attrs.value(QStringLiteral("start")), rep->attrs.value(QStringLiteral("end")),
                                  rep->attrs.value(QStringLiteral("count")));
            if (!mb->childText(u"AlternateEndings").isEmpty())
                extra += QStringLiteral(" ending(%1)").arg(mb->childText(u"AlternateEndings"));
            if (const Node *dir = mb->child(u"Directions"))
                for (const auto &d : dir->children)
                    extra += QStringLiteral(" %1:%2").arg(d->name, d->text.trimmed());
            if (time != lastTime || !extra.isEmpty())
                std::printf("  bar %d: time %s%s\n", bar, qPrintable(time), qPrintable(extra));
            lastTime = time;
        }

    // Tempo changes (automations on the master track).
    std::printf("\nTEMPO\n");
    if (const Node *mt = gp->child(u"MasterTrack"))
        if (const Node *autos = mt->child(u"Automations"))
            for (const auto &a : autos->children)
                if (a->childText(u"Type") == u"Tempo")
                    std::printf("  bar %d pos %s: %s (linear %s)\n", a->childText(u"Bar").toInt() + 1,
                                qPrintable(a->childText(u"Position")), qPrintable(a->childText(u"Value")),
                                qPrintable(a->childText(u"Linear")));

    // The first bars of one track, as string/fret.
    const auto barsById = byId(gp->child(u"Bars"));
    const auto voicesById = byId(gp->child(u"Voices"));
    const auto beatsById = byId(gp->child(u"Beats"));
    const auto notesById = byId(gp->child(u"Notes"));
    const auto rhythmsById = byId(gp->child(u"Rhythms"));
    std::printf("\nTRACK %d, first %d bars (beat: duration then string:fret notes; strings counted from the lowest = 0)\n",
                track, bars);
    bar = 0;
    if (masterBars)
        for (const auto &mb : masterBars->children) {
            if (++bar > bars)
                break;
            const QStringList barIds = mb->childText(u"Bars").split(u' ', Qt::SkipEmptyParts);
            const Node *b = barsById.value(barIds.value(track));
            if (!b)
                continue;
            std::printf("  bar %d:", bar);
            const QStringList voiceIds = b->childText(u"Voices").split(u' ', Qt::SkipEmptyParts);
            for (int v = 0; v < voiceIds.size(); ++v) {
                if (voiceIds[v] == u"-1")
                    continue;
                const Node *voice = voicesById.value(voiceIds[v]);
                if (!voice)
                    continue;
                if (v > 0)
                    std::printf("  | voice %d:", v);
                for (const QString &beatId : voice->childText(u"Beats").split(u' ', Qt::SkipEmptyParts)) {
                    const Node *beat = beatsById.value(beatId);
                    if (!beat)
                        continue;
                    QString dur;
                    if (const Node *r = rhythmsById.value(beat->child(u"Rhythm") ? beat->child(u"Rhythm")->attrs.value(QStringLiteral("ref")) : QString())) {
                        dur = r->childText(u"NoteValue");
                        if (const Node *dot = r->child(u"AugmentationDot"))
                            dur += QString(dot->attrs.value(QStringLiteral("count")).toInt(), u'.');
                        if (const Node *tup = r->child(u"PrimaryTuplet"))
                            dur += QStringLiteral("/%1:%2").arg(tup->attrs.value(QStringLiteral("num")), tup->attrs.value(QStringLiteral("den")));
                    }
                    std::printf(" [%s", qPrintable(dur));
                    const QStringList noteIds = beat->childText(u"Notes").split(u' ', Qt::SkipEmptyParts);
                    if (noteIds.isEmpty())
                        std::printf(" rest");
                    for (const QString &nid : noteIds) {
                        const Node *n = notesById.value(nid);
                        if (!n)
                            continue;
                        const Node *s = n->property(u"String");
                        const Node *fr = n->property(u"Fret");
                        const Node *mn = n->property(u"Midi");
                        const Node *el = n->property(u"Element"); // drum kits
                        QString techniques;
                        if (n->child(u"Tie") && n->child(u"Tie")->attrs.value(QStringLiteral("destination")) == u"true")
                            techniques += u'~';
                        for (const char *p : {"Bended", "Slide", "HopoOrigin", "PalmMuted", "Muted", "HarmonicType"})
                            if (n->property(QString::fromLatin1(p)))
                                techniques += u' ' + QString::fromLatin1(p);
                        if (s && fr)
                            std::printf(" %s:%s%s", qPrintable(s->childText(u"String")), qPrintable(fr->childText(u"Fret")),
                                        qPrintable(techniques));
                        else if (mn || el)
                            std::printf(" midi %s%s", qPrintable(mn ? mn->childText(u"Number") : el->childText(u"Element")),
                                        qPrintable(techniques));
                        else
                            std::printf(" ?");
                    }
                    std::printf("]");
                }
            }
            std::printf("\n");
        }
}

// --- MIDI file ----------------------------------------------------------------

struct Reader
{
    const QByteArray &b;
    qsizetype p = 0;
    bool ok() const { return p <= b.size(); }
    std::uint8_t u8() { return p < b.size() ? std::uint8_t(b[p++]) : (++p, 0); }
    std::uint32_t be(int n)
    {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            v = v << 8 | u8();
        return v;
    }
    std::uint32_t vlq()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const std::uint8_t c = u8();
            v = v << 7 | (c & 0x7F);
            if (!(c & 0x80))
                break;
        }
        return v;
    }
};

void dumpMidi(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "can't open %s\n", qPrintable(path));
        return;
    }
    const QByteArray data = f.readAll();
    Reader r{data};
    std::printf("== %s (%lld bytes)\n", qPrintable(path), static_cast<long long>(data.size()));
    if (r.be(4) != 0x4D546864) {
        std::printf("not a Standard MIDI File\n");
        return;
    }
    const std::uint32_t headerLen = r.be(4);
    const int format = int(r.be(2)), tracks = int(r.be(2)), division = int(r.be(2));
    r.p = 8 + qsizetype(headerLen);
    std::printf("format %d, %d tracks, division %d %s\n", format, tracks, division,
                division & 0x8000 ? "(SMPTE)" : "ticks per quarter");
    for (int t = 0; t < tracks && r.p + 8 <= data.size(); ++t) {
        if (r.be(4) != 0x4D54726B) {
            std::printf("  bad track chunk\n");
            return;
        }
        const qsizetype end = r.p + 4 + qsizetype(r.be(4));
        std::uint32_t tick = 0;
        std::uint8_t status = 0;
        QString name, instrument;
        std::set<int> channels, programs;
        int notes = 0, lo = 128, hi = -1, bends = 0, ccs = 0;
        std::vector<std::pair<std::uint32_t, int>> first;
        std::vector<QString> meta;
        while (r.p < end) {
            tick += r.vlq();
            std::uint8_t s = r.u8();
            if (s < 0x80) { // running status
                --r.p;
                s = status;
            } else if (s < 0xF0) {
                status = s;
            }
            if (s == 0xFF) {
                const int type = r.u8();
                const std::uint32_t len = r.vlq();
                const QByteArray body = data.mid(r.p, len);
                r.p += len;
                switch (type) {
                case 0x03: name = QString::fromLatin1(body); break;
                case 0x04: instrument = QString::fromLatin1(body); break;
                case 0x51:
                    meta.push_back(QStringLiteral("tick %1 tempo %2 BPM").arg(tick).arg(
                        60000000.0 / ((std::uint8_t(body[0]) << 16) | (std::uint8_t(body[1]) << 8) | std::uint8_t(body[2])), 0, 'f', 2));
                    break;
                case 0x58:
                    meta.push_back(QStringLiteral("tick %1 time %2/%3").arg(tick).arg(int(std::uint8_t(body[0]))).arg(1 << std::uint8_t(body[1])));
                    break;
                case 0x05: break; // lyrics: never printed
                default: break;
                }
            } else if (s == 0xF0 || s == 0xF7) {
                r.p += r.vlq();
            } else {
                const int type = s & 0xF0, ch = s & 0x0F;
                const int d1 = r.u8();
                const int d2 = (type == 0xC0 || type == 0xD0) ? 0 : r.u8();
                channels.insert(ch + 1);
                if (type == 0x90 && d2 > 0) {
                    ++notes;
                    lo = std::min(lo, d1);
                    hi = std::max(hi, d1);
                    if (first.size() < 12)
                        first.emplace_back(tick, d1);
                } else if (type == 0xE0) {
                    ++bends;
                } else if (type == 0xB0) {
                    ++ccs;
                } else if (type == 0xC0) {
                    programs.insert(d1);
                }
            }
        }
        r.p = end;
        std::printf("  track %d \"%s\"%s  channels", t, qPrintable(name),
                    instrument.isEmpty() ? "" : qPrintable(QStringLiteral(" (%1)").arg(instrument)));
        for (int c : channels)
            std::printf(" %d", c);
        std::printf("  programs");
        for (int p : programs)
            std::printf(" %d", p);
        std::printf("  notes %d", notes);
        if (notes)
            std::printf(" (%s%d..%s%d)", noteName(lo), lo / 12 - 1, noteName(hi), hi / 12 - 1);
        std::printf("  bends %d  CCs %d  length %u ticks\n", bends, ccs, tick);
        for (const QString &m : meta)
            std::printf("      %s\n", qPrintable(m));
        if (!first.empty()) {
            std::printf("      first notes:");
            for (const auto &[tk, n] : first)
                std::printf(" %u:%s%d", tk, noteName(n), n / 12 - 1);
            std::printf("\n");
        }
    }

    // What Krate's reader (krate_song) makes of it.
    const auto result = krate::song::readMidiFile(path.toStdString());
    if (!result.song) {
        std::printf("\nKrate's reader: %s\n", result.error.c_str());
        return;
    }
    const krate::song::Song &song = *result.song;
    const auto end = song.tempo.barAt(std::max<krate::song::Tick>(0, song.length - 1)); // the last bar with something in it
    std::printf("\nKrate's reader: \"%s\", %zu parts, %d bars, %.1f s, %zu tempo changes, %zu time signatures\n",
                song.title.c_str(), song.tracks.size(), end.bar + 1, song.tempo.secondsAt(song.length),
                song.tempo.tempos().size(), song.tempo.timeSignatures().size());
    for (const auto &t : song.tracks)
        std::printf("  \"%s\" ch %d%s program %d: %d notes, %zu events\n", t.name.c_str(), t.channel + 1,
                    t.drums ? " (drums)" : "", t.program, t.noteCount, t.events.size());
}

// Krate's .gp reader against a MIDI file of the same song: per track, how
// many note-ons match (same pitch, within a 64th note), in quarter-note time.
void compare(const QString &gpPath, const QString &midPath)
{
    const auto gp = krate::song::readGpFile(gpPath.toStdString());
    const auto mid = krate::song::readMidiFile(midPath.toStdString());
    if (!gp.song || !mid.song) {
        std::printf("can't compare: %s %s\n", gp.error.c_str(), mid.error.c_str());
        return;
    }
    const krate::song::Song &a = *gp.song, &b = *mid.song;
    std::printf("tab: %zu tracks, %zu bars played (%zu written), %.1f s | MIDI: %zu tracks, %.1f s\n", a.tracks.size(),
                gp.score->playOrder.size(), gp.score->bars.size(), a.tempo.secondsAt(a.length), b.tracks.size(),
                b.tempo.secondsAt(b.length));
    const auto onsets = [](const krate::song::Song &s, const krate::song::Track &t) {
        std::multimap<int, double> out; // pitch -> beat
        for (const auto &e : t.events)
            if (e.type() == 0x90)
                out.emplace(e.data1, s.tempo.beatAt(e.tick));
        return out;
    };
    for (std::size_t i = 0; i < std::min(a.tracks.size(), b.tracks.size()); ++i) {
        auto x = onsets(a, a.tracks[i]);
        auto y = onsets(b, b.tracks[i]);
        const std::size_t tabNotes = x.size();
        int matched = 0;
        double worst = 0.0;
        for (auto it = x.begin(); it != x.end(); ++it) {
            auto [lo, hi] = y.equal_range(it->first);
            auto best = y.end();
            for (auto j = lo; j != hi; ++j)
                if (best == y.end() || std::abs(j->second - it->second) < std::abs(best->second - it->second))
                    best = j;
            if (best != y.end() && std::abs(best->second - it->second) <= 1.0 / 16.0) {
                worst = std::max(worst, std::abs(best->second - it->second));
                ++matched;
                y.erase(best);
            }
        }
        std::printf("  [%zu] %-40.40s tab %5zu notes, MIDI %5zu: %5d match (%.1f%%), worst offset %.4f beats, MIDI-only %zu\n",
                    i, a.tracks[i].name.c_str(), tabNotes, std::size_t(b.tracks[i].noteCount), matched,
                    tabNotes == 0 ? 100.0 : 100.0 * matched / double(tabNotes), worst, y.size());
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int bars = 4, track = 0;
    QString xmlOut;
    QStringList files;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == u"--bars" && i + 1 < args.size())
            bars = args[++i].toInt();
        else if (args[i] == u"--track" && i + 1 < args.size())
            track = args[++i].toInt();
        else if (args[i] == u"--xml" && i + 1 < args.size())
            xmlOut = args[++i];
        else if (args[i] == u"--compare")
            continue;
        else
            files << args[i];
    }
    if (files.size() == 2 && args.contains(QStringLiteral("--compare"))) {
        compare(files[0], files[1]);
        return 0;
    }
    if (files.isEmpty()) {
        std::fprintf(stderr, "usage: song-dump [--bars N] [--track T] [--xml OUT] FILE.gp|FILE.mid ...\n");
        return 1;
    }
    for (const QString &file : files) {
        if (file.endsWith(u".mid", Qt::CaseInsensitive) || file.endsWith(u".midi", Qt::CaseInsensitive))
            dumpMidi(file);
        else
            dumpGp(file, bars, track, xmlOut);
        std::printf("\n");
    }
    return 0;
}
