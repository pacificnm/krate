#include "konnekt/app/Explainer.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include "konnekt/dsp/Modules.h"

namespace konnekt {

namespace {
QString text(const char *s) { return QString::fromUtf8(s); }
QString number(double v, int decimals) { return QString::number(v, 'f', decimals); }

const char *const kSignalKeys[] = {"audio", "pitch", "gate", "mod"};
const char *const kSignalNames[] = {"Audio (orange)", "Pitch (blue)", "Gate (green)", "Modulation (violet)"};

// The text sections, in the order a card shows them.
const struct
{
    const char *field, *heading;
} kSections[] = {{"what", "What it is"},       {"does", "How it works"}, {"why", "Why you need it"},
                 {"where", "Where it goes"},   {"try", "Try this"}};

// A text is a string, or a list shown as bullets.
QString paragraph(const QJsonValue &v)
{
    if (!v.isArray())
        return v.toString();
    QStringList lines;
    for (const QJsonValue &line : v.toArray())
        lines << QStringLiteral("•  ") + line.toString();
    return lines.join(u'\n');
}

// "pitch3" -> "pitch#", "3"; else empty.
std::pair<QString, QString> numbered(const QString &key)
{
    static const QRegularExpression re(QStringLiteral("^(.*?)(\\d+)$"));
    const QRegularExpressionMatch m = re.match(key);
    return m.hasMatch() ? std::pair{m.captured(1) + u'#', m.captured(2)} : std::pair{QString(), QString()};
}

// Every string in `o` with "#" replaced by `n` (for numbered parts: step 3, input 2).
QJsonObject withNumber(const QJsonObject &o, const QString &n)
{
    QJsonObject out;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (it->isString())
            out.insert(it.key(), it->toString().replace(u'#', n));
        else if (it->isArray()) {
            QJsonArray a;
            for (const QJsonValue &v : it->toArray())
                a << (v.isString() ? QJsonValue(v.toString().replace(u'#', n)) : v);
            out.insert(it.key(), a);
        } else
            out.insert(it.key(), *it);
    }
    return out;
}

template <typename List> int indexOf(const List &list, const QString &key)
{
    for (std::size_t i = 0; i < list.size(); ++i)
        if (key == text(list[i].key))
            return int(i);
    return -1;
}

// What a path names.
struct Target
{
    enum Kind { None, Module, Knob, In, Out, Signal, App } kind = None;
    const ModuleInfo *info = nullptr;
    int slot = -1;  // on the rack, or -1 for the type
    int index = -1; // the knob or jack
    QString key;    // signal or app item
};

const char *partName(Target::Kind k) { return k == Target::Knob ? "knob" : k == Target::In ? "in" : "out"; }
QString kindKey(Target::Kind k) { return QString::fromLatin1(k == Target::Knob ? "knobs" : k == Target::In ? "inputs" : "outputs"); }

Target parse(const QString &path, const Explainer::Rack &rack)
{
    const QStringList p = path.split(u'/');
    Target t;
    if (p.size() == 2 && p[0] == u"signal") {
        for (int s = 0; s < 4; ++s)
            if (p[1] == text(kSignalKeys[s])) {
                t.kind = Target::Signal;
                t.index = s;
                t.key = p[1];
            }
        return t;
    }
    if (p.size() == 2 && p[0] == u"app") {
        t.kind = Target::App;
        t.key = p[1];
        return t;
    }
    if ((p.size() != 2 && p.size() != 4) || (p[0] != u"t" && p[0] != u"m"))
        return t;
    if (p[0] == u"m") {
        bool ok = false;
        t.slot = p[1].toInt(&ok);
        const Module *m = ok ? rack.module(t.slot) : nullptr;
        t.info = m ? &m->info() : nullptr;
    } else
        t.info = findModule(p[1].toStdString());
    if (!t.info)
        return {};
    if (p.size() == 2) {
        t.kind = Target::Module;
        return t;
    }
    if (p[2] == u"knob") {
        t.kind = Target::Knob;
        t.index = indexOf(t.info->knobs, p[3]);
    } else if (p[2] == u"in") {
        t.kind = Target::In;
        t.index = indexOf(t.info->inputs, p[3]);
    } else if (p[2] == u"out") {
        t.kind = Target::Out;
        t.index = indexOf(t.info->outputs, p[3]);
    }
    return t.index >= 0 ? t : Target{};
}

// The path of a part of the module `t` names (its slot's, or its type's).
QString partPath(const Target &t, Target::Kind kind, const char *key)
{
    const QString base = t.slot >= 0 ? QStringLiteral("m/%1").arg(t.slot) : QStringLiteral("t/") + text(t.info->type);
    return kind == Target::Module ? base : base + u'/' + text(partName(kind)) + u'/' + text(key);
}

// A link in the texts ("vca/in/cv", "signal/gate") as a path from `from`:
// into the same type, it stays on the same module.
QString resolve(const QString &link, const Target &from)
{
    if (link.startsWith(u"signal/") || link.startsWith(u"app/"))
        return link;
    if (from.slot >= 0 && from.info && link.section(u'/', 0, 0) == text(from.info->type))
        return QStringLiteral("m/%1").arg(from.slot) + link.mid(link.indexOf(u'/') < 0 ? link.size() : link.indexOf(u'/'));
    return QStringLiteral("t/") + link;
}

QVariantMap fact(const QString &label, const QString &value, const QString &link = {})
{
    return {{QStringLiteral("label"), label}, {QStringLiteral("text"), value}, {QStringLiteral("link"), link}};
}
} // namespace

QString knobValueText(const KnobInfo &info, double position)
{
    if (!info.labels.empty())
        return text(info.labels[std::size_t(info.index(float(position)))]);
    const double v = info.value(float(position));
    const std::string_view unit = info.unit;
    if (unit == "Hz")
        return v >= 1000.0 ? number(v / 1000.0, 2) + QStringLiteral(" kHz") : number(v, v < 10.0 ? 2 : 0) + QStringLiteral(" Hz");
    if (unit == "s")
        return v < 1.0 ? number(v * 1000.0, v < 0.01 ? 1 : 0) + QStringLiteral(" ms") : number(v, 2) + QStringLiteral(" s");
    if (unit == "st")
        return (v > 0.0 ? QStringLiteral("+") : QString()) + number(v, info.detents > 1 ? 0 : 1) + QStringLiteral(" st");
    if (unit == "oct")
        return (info.detents > 1 && v > 0.0 ? QStringLiteral("+") : QString()) + number(v, info.detents > 1 ? 0 : 2) + QStringLiteral(" oct");
    if (unit == "BPM")
        return number(v, 0) + QStringLiteral(" BPM");
    if (info.detents > 1)
        return number(v, 0);
    if (info.min >= 0.0f && info.max <= 1.0f)
        return number(v * 100.0, 0) + u'%';
    return number(v, 2);
}

bool Explainer::load(const QString &dir)
{
    m_modules = m_signals = m_app = {};
    const QStringList files = QDir(dir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        QFile f(dir + u'/' + name);
        if (!f.open(QIODevice::ReadOnly))
            return false;
        QJsonParseError error;
        const QJsonObject json = QJsonDocument::fromJson(f.readAll(), &error).object();
        if (error.error != QJsonParseError::NoError)
            return false;
        const auto merge = [](QJsonObject &into, const QJsonObject &from) {
            for (auto it = from.begin(); it != from.end(); ++it)
                into.insert(it.key(), *it);
        };
        merge(m_modules, json.value(u"modules").toObject());
        merge(m_signals, json.value(u"signals").toObject());
        merge(m_app, json.value(u"app").toObject());
    }
    return !files.isEmpty();
}

QJsonObject Explainer::moduleEntry(const QString &type) const { return m_modules.value(type).toObject(); }

// A part's texts: its own, or a numbered part's ("pitch#"), or the module's
// it's "like" (the filters share their knobs).
QJsonObject Explainer::entry(const ModuleInfo &info, const QString &kind, const QString &key) const
{
    QString type = text(info.type);
    for (int depth = 0; depth < 4 && !type.isEmpty(); ++depth) {
        const QJsonObject module = moduleEntry(type);
        const QJsonObject parts = module.value(kind).toObject();
        if (parts.contains(key))
            return parts.value(key).toObject();
        if (const auto [pattern, n] = numbered(key); !pattern.isEmpty() && parts.contains(pattern))
            return withNumber(parts.value(pattern).toObject(), n);
        type = module.value(u"like").toString();
    }
    return {};
}

bool Explainer::linkResolves(const QString &link) const
{
    if (link.startsWith(u"app/"))
        return m_app.contains(link.mid(4));
    if (link.startsWith(u"signal/"))
        return m_signals.contains(link.mid(7)) && parse(link, {}).kind == Target::Signal;
    return parse(QStringLiteral("t/") + link, {}).kind != Target::None;
}

QVariantMap Explainer::card(const QString &path, const Rack &rack) const
{
    const Target t = parse(path, rack);
    if (t.kind == Target::None)
        return {};

    QVariantMap card{{QStringLiteral("path"), path}, {QStringLiteral("signal"), -1}, {QStringLiteral("scopeSlot"), -1},
                     {QStringLiteral("scopeJack"), -1}};
    QJsonObject texts;
    QVariantList facts, groups;

    const auto linkTo = [&](const QString &p) -> QVariantMap { // a chip: its label and colour
        const Target to = parse(p, rack);
        QString label;
        int signal = -1;
        switch (to.kind) {
        case Target::Module: label = text(to.info->name); break;
        case Target::Knob: label = text(to.info->name) + QStringLiteral(" · ") + text(to.info->knobs[std::size_t(to.index)].name); break;
        case Target::In:
        case Target::Out: {
            const JackInfo &j = (to.kind == Target::In ? to.info->inputs : to.info->outputs)[std::size_t(to.index)];
            label = text(to.info->name) + QStringLiteral(" · ") + text(j.name) + (to.kind == Target::In ? QStringLiteral(" in") : QStringLiteral(" out"));
            signal = int(j.signal);
            break;
        }
        case Target::Signal: label = text(kSignalNames[to.index]) + QStringLiteral(" signals"); signal = to.index; break;
        case Target::App: label = m_app.value(to.key).toObject().value(u"title").toString(); break;
        case Target::None: break;
        }
        return {{QStringLiteral("path"), p}, {QStringLiteral("label"), label}, {QStringLiteral("signal"), signal}};
    };
    const auto partLinks = [&](Target::Kind kind) {
        QVariantList links;
        if (kind == Target::Knob)
            for (const KnobInfo &k : t.info->knobs)
                links << linkTo(partPath(t, kind, k.key));
        else
            for (const JackInfo &j : kind == Target::In ? t.info->inputs : t.info->outputs)
                links << linkTo(partPath(t, kind, j.key));
        return links;
    };
    const auto group = [&](const QString &heading, const QVariantList &links) {
        if (!links.isEmpty())
            groups << QVariantMap{{QStringLiteral("heading"), heading}, {QStringLiteral("links"), links}};
    };
    const auto runs = [&] {
        return t.info->perVoice ? QStringLiteral("Once per voice: each note gets its own")
                                : QStringLiteral("Once for all voices: per-voice cables into it are mixed");
    };

    if (t.info) {
        card.insert(QStringLiteral("module"), text(t.info->name));
        card.insert(QStringLiteral("moduleId"), partPath(t, Target::Module, ""));
    }

    switch (t.kind) {
    case Target::Module: {
        texts = moduleEntry(text(t.info->type));
        card.insert(QStringLiteral("title"), text(t.info->name));
        card.insert(QStringLiteral("kind"), QStringLiteral("Module · ") + text(t.info->category));
        card.insert(QStringLiteral("lead"), text(t.info->help));
        facts << fact(QStringLiteral("Runs"), runs());
        if (t.slot >= 0) {
            int in = 0, out = 0;
            for (const Rack::Cable &c : rack.cables) {
                in += c.toSlot == t.slot;
                out += c.fromSlot == t.slot;
            }
            facts << fact(QStringLiteral("Cables"), QStringLiteral("%1 in, %2 out").arg(in).arg(out));
        }
        group(QStringLiteral("Knobs and buttons"), partLinks(Target::Knob));
        group(QStringLiteral("Inputs"), partLinks(Target::In));
        group(QStringLiteral("Outputs"), partLinks(Target::Out));
        break;
    }
    case Target::Knob: {
        const KnobInfo &k = t.info->knobs[std::size_t(t.index)];
        texts = entry(*t.info, kindKey(t.kind), text(k.key));
        card.insert(QStringLiteral("title"), text(k.name));
        card.insert(QStringLiteral("kind"), k.detents == 2 ? QStringLiteral("Button") : k.detents > 2 && !k.labels.empty() ? QStringLiteral("Selector") : QStringLiteral("Knob"));
        card.insert(QStringLiteral("lead"), text(k.help));
        const Module *m = t.slot >= 0 ? rack.module(t.slot) : nullptr;
        if (m)
            facts << fact(QStringLiteral("Now"), knobValueText(k, m->knobPosition(t.index)));
        if (!k.labels.empty()) {
            QStringList labels;
            for (const char *l : k.labels)
                labels << text(l);
            facts << fact(QStringLiteral("Choices"), labels.join(QStringLiteral(", ")));
        } else
            facts << fact(QStringLiteral("Range"), knobValueText(k, 0.0) + QStringLiteral(" to ") + knobValueText(k, 1.0));
        facts << fact(QStringLiteral("Default"), knobValueText(k, k.position(k.defaultValue)));
        for (const JackInfo &j : t.info->inputs) {
            if (j.amount == t.index)
                facts << fact(QStringLiteral("Works with"), QStringLiteral("Sets how much the %1 input does").arg(text(j.name).toUpper()),
                              partPath(t, Target::In, j.key));
            if (j.knob == t.index)
                facts << fact(QStringLiteral("Works with"),
                              QStringLiteral("What the %1 input reads with nothing plugged in; added to what is").arg(text(j.name).toUpper()),
                              partPath(t, Target::In, j.key));
        }
        if (m) {
            const int cc = rack.learnedCc(t.slot, t.index);
            facts << fact(QStringLiteral("MIDI"), cc >= 0 ? QStringLiteral("Linked to CC %1").arg(cc)
                                                          : QStringLiteral("Not linked: LEARN links it to a knob on your controller"),
                          cc >= 0 ? QString() : QStringLiteral("app/learn"));
        }
        break;
    }
    case Target::In:
    case Target::Out: {
        const bool in = t.kind == Target::In;
        const JackInfo &j = (in ? t.info->inputs : t.info->outputs)[std::size_t(t.index)];
        texts = entry(*t.info, kindKey(t.kind), text(j.key));
        card.insert(QStringLiteral("title"), text(j.name));
        card.insert(QStringLiteral("kind"), in ? QStringLiteral("Input jack") : QStringLiteral("Output jack"));
        card.insert(QStringLiteral("lead"), text(j.help));
        card.insert(QStringLiteral("signal"), int(j.signal));
        facts << fact(QStringLiteral("Carries"), text(kSignalNames[int(j.signal)]), QStringLiteral("signal/") + text(kSignalKeys[int(j.signal)]));
        if (in) {
            if (t.slot >= 0) {
                const auto c = std::find_if(rack.cables.begin(), rack.cables.end(),
                                            [&](const Rack::Cable &c) { return c.toSlot == t.slot && c.toJack == t.index; });
                const Module *from = c != rack.cables.end() ? rack.module(c->fromSlot) : nullptr;
                if (from) {
                    const JackInfo &o = from->info().outputs[std::size_t(c->fromJack)];
                    facts << fact(QStringLiteral("Plugged in"), text(from->info().name) + QStringLiteral(" · ") + text(o.name),
                                  QStringLiteral("m/%1/out/").arg(c->fromSlot) + text(o.key));
                } else
                    facts << fact(QStringLiteral("Plugged in"),
                                  j.knob >= 0 ? QStringLiteral("Nothing: it reads its %1 knob").arg(text(t.info->knobs[std::size_t(j.knob)].name).toUpper())
                                              : QStringLiteral("Nothing: it reads 0"));
            }
            if (j.amount >= 0)
                facts << fact(QStringLiteral("Scaled by"), text(t.info->knobs[std::size_t(j.amount)].name).toUpper() + QStringLiteral(" knob"),
                              partPath(t, Target::Knob, t.info->knobs[std::size_t(j.amount)].key));
            if (j.knob >= 0)
                facts << fact(QStringLiteral("Added to"), text(t.info->knobs[std::size_t(j.knob)].name).toUpper() + QStringLiteral(" knob"),
                              partPath(t, Target::Knob, t.info->knobs[std::size_t(j.knob)].key));
        } else {
            facts << fact(QStringLiteral("Runs"), t.info->perVoice ? QStringLiteral("One signal per voice") : QStringLiteral("One signal for all voices"));
            if (t.slot >= 0) {
                int n = 0;
                for (const Rack::Cable &c : rack.cables)
                    if (c.fromSlot == t.slot && c.fromJack == t.index)
                        if (const Module *to = rack.module(c.toSlot)) {
                            const JackInfo &i = to->info().inputs[std::size_t(c.toJack)];
                            facts << fact(n++ == 0 ? QStringLiteral("Goes to") : QString(), text(to->info().name) + QStringLiteral(" · ") + text(i.name),
                                          QStringLiteral("m/%1/in/").arg(c.toSlot) + text(i.key));
                        }
                if (n == 0)
                    facts << fact(QStringLiteral("Goes to"), QStringLiteral("Nothing yet: click it, then click an input"));
                card.insert(QStringLiteral("scopeSlot"), t.slot);
                card.insert(QStringLiteral("scopeJack"), t.index);
            }
        }
        break;
    }
    case Target::Signal:
        texts = m_signals.value(t.key).toObject();
        card.insert(QStringLiteral("title"), texts.value(u"title").toString());
        card.insert(QStringLiteral("kind"), QStringLiteral("Signal"));
        card.insert(QStringLiteral("signal"), t.index);
        break;
    case Target::App:
        texts = m_app.value(t.key).toObject();
        if (texts.isEmpty())
            return {};
        card.insert(QStringLiteral("title"), texts.value(u"title").toString());
        card.insert(QStringLiteral("kind"), QStringLiteral("Konnekt"));
        break;
    case Target::None: break;
    }

    QVariantList sections;
    for (const auto &s : kSections)
        if (const QString body = paragraph(texts.value(QLatin1String(s.field))); !body.isEmpty())
            sections << QVariantMap{{QStringLiteral("heading"), text(s.heading)}, {QStringLiteral("text"), body}};
    QVariantList see;
    for (const QJsonValue &link : texts.value(u"see").toArray())
        see << linkTo(resolve(link.toString(), t));
    group(QStringLiteral("See also"), see);

    card.insert(QStringLiteral("sections"), sections);
    card.insert(QStringLiteral("facts"), facts);
    card.insert(QStringLiteral("groups"), groups);
    return card;
}

QStringList Explainer::problems() const
{
    QStringList out;
    const auto checkLinks = [&](const QString &where, const QJsonObject &o) {
        for (const QJsonValue &link : o.value(u"see").toArray())
            if (!linkResolves(link.toString()))
                out << where + QStringLiteral(": link to nothing: ") + link.toString();
    };
    for (const ModuleInfo *info : moduleTypes()) {
        const QString type = text(info->type);
        const QJsonObject module = moduleEntry(type);
        for (const char *field : {"what", "does", "why", "where", "try"})
            if (module.value(QLatin1String(field)).toString().isEmpty() && !module.value(QLatin1String(field)).isArray())
                out << type + QStringLiteral(": no ") + text(field);
        checkLinks(type, module);
        const auto checkPart = [&](const QString &kind, const char *key) {
            const QJsonObject e = entry(*info, kind, text(key));
            if (e.value(u"what").toString().isEmpty())
                out << type + u'/' + kind + u'/' + text(key) + QStringLiteral(": no what");
            if (e.value(u"why").toString().isEmpty())
                out << type + u'/' + kind + u'/' + text(key) + QStringLiteral(": no why");
            checkLinks(type + u'/' + kind + u'/' + text(key), e);
        };
        for (const KnobInfo &k : info->knobs)
            checkPart(QStringLiteral("knobs"), k.key);
        for (const JackInfo &j : info->inputs)
            checkPart(QStringLiteral("inputs"), j.key);
        for (const JackInfo &j : info->outputs)
            checkPart(QStringLiteral("outputs"), j.key);
    }
    // Texts for things that don't exist (a typo'd key is a part with no text).
    for (auto it = m_modules.begin(); it != m_modules.end(); ++it) {
        const ModuleInfo *info = findModule(it.key().toStdString());
        if (!info) {
            out << it.key() + QStringLiteral(": no such module");
            continue;
        }
        if (const QString like = it->toObject().value(u"like").toString(); !like.isEmpty() && !findModule(like.toStdString()))
            out << it.key() + QStringLiteral(": like no such module: ") + like;
        const auto exists = [&](const QString &kind, const QString &key) {
            const auto match = [&](const char *k) { return key == text(k) || numbered(text(k)).first == key; };
            if (kind == u"knobs")
                return std::any_of(info->knobs.begin(), info->knobs.end(), [&](const KnobInfo &k) { return match(k.key); });
            const auto &list = kind == u"inputs" ? info->inputs : info->outputs;
            return std::any_of(list.begin(), list.end(), [&](const JackInfo &j) { return match(j.key); });
        };
        for (const QString &kind : {QStringLiteral("knobs"), QStringLiteral("inputs"), QStringLiteral("outputs")}) {
            const QJsonObject parts = it->toObject().value(kind).toObject();
            for (auto p = parts.begin(); p != parts.end(); ++p)
                if (!exists(kind, p.key()))
                    out << it.key() + u'/' + kind + u'/' + p.key() + QStringLiteral(": no such part");
        }
    }
    for (const char *s : kSignalKeys) {
        const QJsonObject e = m_signals.value(text(s)).toObject();
        if (e.value(u"title").toString().isEmpty() || e.value(u"what").toString().isEmpty())
            out << QStringLiteral("signal/") + text(s) + QStringLiteral(": no title or what");
        checkLinks(QStringLiteral("signal/") + text(s), e);
    }
    for (auto it = m_app.begin(); it != m_app.end(); ++it) {
        if (it->toObject().value(u"title").toString().isEmpty() || it->toObject().value(u"what").toString().isEmpty())
            out << QStringLiteral("app/") + it.key() + QStringLiteral(": no title or what");
        checkLinks(QStringLiteral("app/") + it.key(), it->toObject());
    }
    return out;
}

} // namespace konnekt
