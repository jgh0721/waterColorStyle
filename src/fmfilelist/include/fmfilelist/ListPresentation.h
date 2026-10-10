#pragma once
#include "ListAppearance.h"
#include <QFontMetrics>
#include <QString>
#include <algorithm>

namespace fm::filelist {
// Compare the one-line candidate, even while displaying two lines. Otherwise
// measuring the expanded name cell makes Auto oscillate on every resize.
inline bool ResolveAutoTwoLine(int Width, double ClippedPercent, int WidthLimit, int PercentLimit,
    bool PreviousTwoLine, bool Initialized)
{
    if (Width <= 0) return PreviousTwoLine;
    if (Initialized && PreviousTwoLine)
        return Width < WidthLimit + 32 || (ClippedPercent > 0 && ClippedPercent >= std::max(0, PercentLimit - 5));
    return Width < WidthLimit || ClippedPercent >= PercentLimit;
}

inline QString ElideFileName(const QString& Name, const QFontMetrics& Metrics, int Width,
    NameElide Mode = NameElide::MiddleKeepExtension, bool Directory = false)
{
    if (Width <= 0) return {};
    if (Mode != NameElide::MiddleKeepExtension || Directory)
        return Metrics.elidedText(Name, Mode == NameElide::End ? Qt::ElideRight : Qt::ElideMiddle, Width);
    int Dot = Name.lastIndexOf('.');
    for (const auto& Suffix : {QStringLiteral(".tar.gz"), QStringLiteral(".tar.bz2"), QStringLiteral(".tar.xz"), QStringLiteral(".tar.zst")})
        if (Name.endsWith(Suffix, Qt::CaseInsensitive)) { Dot = int(Name.size() - Suffix.size()); break; }
    if (Dot <= 0 || Dot == Name.size() - 1) return Metrics.elidedText(Name, Qt::ElideMiddle, Width);
    const auto Extension = Name.mid(Dot);
    const int Remaining = Width - Metrics.horizontalAdvance(Extension);
    if (Remaining < Metrics.horizontalAdvance(QString(QChar(0x2026))))
        return Metrics.elidedText(Name, Qt::ElideMiddle, Width);
    return Metrics.elidedText(Name.left(Dot), Qt::ElideMiddle, Remaining) + Extension;
}
}
