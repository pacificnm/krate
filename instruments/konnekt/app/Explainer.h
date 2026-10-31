#pragma once

#include <functional>
#include <vector>

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "konnekt/dsp/Module.h"

namespace konnekt {

// A knob's value as the panel shows it: "1.20 kHz", "+2 oct", "Saw up", "35%".
QString knobValueText(const KnobInfo &knob, double position);

// The explanations a right-click opens (docs/MANUAL.md#explanations): what a
// module, knob or jack is, how it works, why you'd want it, where it goes and
// what to try, from the texts in explain/*.json; with what it is doing on the
// rack right now (its value, what's plugged in, where its cable goes).
//
// An item is named by a path:
//   t/<type>[/knob|in|out/<key>]    a module type, or one of its parts
//   m/<slot>[/knob|in|out/<key>]    the module in that slot on the rack
//   signal/<audio|pitch|gate|mod>   what a jack's colour means
//   app/<name>                      Konnekt's own controls (the top bar)
// The texts' links ("see") name types without the t/ ("vca/in/cv"); on a
// module on the rack, a link into its own type stays on that module.
class Explainer
{
public:
    // What's on the rack, for the live facts.
    struct Rack
    {
        struct Cable
        {
            int fromSlot, fromJack, toSlot, toJack;
        };
        std::function<const Module *(int slot)> module = [](int) { return nullptr; };
        std::vector<Cable> cables;
        std::function<int(int slot, int knob)> learnedCc = [](int, int) { return -1; };
    };

    // Reads every *.json in `dir` (the app's resources; tests: the source folder).
    bool load(const QString &dir);

    // The card for `path`: {path, title, kind, module, moduleId, signal (-1 if none),
    // lead, sections [{heading, text}], facts [{label, text, link}],
    // groups [{heading, links [{path, label, signal}]}], scopeSlot, scopeJack};
    // empty for a path that names nothing.
    QVariantMap card(const QString &path, const Rack &rack) const;

    // Checks, for the tests: parts with no text, texts for parts that don't
    // exist, links that lead nowhere.
    QStringList problems() const;

private:
    QJsonObject entry(const ModuleInfo &info, const QString &kind, const QString &key) const;
    QJsonObject moduleEntry(const QString &type) const;
    bool linkResolves(const QString &link) const;

    QJsonObject m_modules, m_signals, m_app;
};

} // namespace konnekt
