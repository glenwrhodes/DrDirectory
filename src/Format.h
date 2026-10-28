#pragma once
#include <QDateTime>
#include <QLocale>
#include <QString>

namespace Fmt {
inline QString size(qint64 bytes)
{
    if (bytes < 1024)
        return QLocale::system().toString(bytes) + QStringLiteral(" B");
    return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}
inline QString count(qint64 n) { return QLocale::system().toString(n); }
inline QString dateTime(qint64 msecs)
{
    return msecs > 0 ? QDateTime::fromMSecsSinceEpoch(msecs).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                     : QString();
}
inline QString percent(double fraction)
{
    return QString::number(fraction * 100.0, 'f', 1) + QStringLiteral(" %");
}
} // namespace Fmt
