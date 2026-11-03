#pragma once

#include <optional>

#include <QJsonObject>
#include <QString>

#include "dsp/Head.h"

namespace krunch {

// An amp head as a file (docs/HEADS.md): its design, with its name and where
// it came from (the capture it was fitted to, how close). Values out of
// range are brought in, so a hand-edited file can't make the amp unstable.
struct HeadFile
{
    static constexpr int kVersion = 1;

    QString name;
    QJsonObject fit; // {capture, takes, score, start, date}: how it was made
    HeadDesign design;

    QJsonObject toJson() const;
    // nullopt and *error when it isn't a head.
    static std::optional<HeadFile> fromJson(const QJsonObject &json, QString *error = nullptr);
    static std::optional<HeadFile> read(const QString &path, QString *error = nullptr);
    bool write(const QString &path) const;
};

// A built-in head made explicit: its tubes and power amp the ones given (a
// starting point for a head of its own).
HeadDesign explicitHead(Voice voice, PreTube tube, PowerTube power);

} // namespace krunch
