#ifndef GXDE_TOP_PANEL_CLOCKFORMAT_H
#define GXDE_TOP_PANEL_CLOCKFORMAT_H

#include <QDateTime>
#include <QLocale>

namespace ClockFormat {
inline QString normalize(QString pattern)
{
    pattern = pattern.left(256);
    pattern.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    pattern.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QString result;
    bool newline = false;
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern[i];
        const bool escaped = c == QLatin1Char('\\') && i + 1 < pattern.size();
        if (escaped && pattern[i + 1] == QLatin1Char('\n')) {
            result += QStringLiteral("\\\\");
            continue;
        }
        if (c == QLatin1Char('\n') || (escaped && pattern[i + 1] == QLatin1Char('n'))) {
            result += newline ? QStringLiteral(" ") : QStringLiteral("\\n");
            newline = true;
            if (escaped)
                ++i;
        } else {
            result += c;
            if (escaped)
                result += pattern[++i];
        }
    }
    return result;
}

inline QString render(const QString &pattern, const QDateTime &now,
        const QLocale &locale = QLocale(), bool use12Hour = false) {
    const QString input = normalize(pattern);
    QString result;
    bool lineHasHour = false;
    qsizetype timeEnd = -1;
    auto finishTimeLine = [&] {
        if (use12Hour && lineHasHour) {
            result.insert(timeEnd, now.time().hour() < 12
                ? QStringLiteral(" A.M.") : QStringLiteral(" P.M."));
        }
        lineHasHour = false;
        timeEnd = -1;
    };
    for (int i = 0; i < input.size(); ++i) {
        const QChar c = input[i];
        if (c == QLatin1Char('\\') && i + 1 < input.size()) {
            const QChar escaped = input[++i];
            if (escaped == QLatin1Char('n')) {
                finishTimeLine();
            }

            result += escaped == QLatin1Char('n') ? QLatin1Char('\n') : escaped;
            continue;
        }
        if (input.mid(i, 3) == QLatin1String("ddd")) {
            result += locale.dayName(now.date().dayOfWeek(), QLocale::ShortFormat);
            i += 2;
            continue;
        }
        int value = -1;
        if (c == QLatin1Char('Y')) value = now.date().year();
        else if (c == QLatin1Char('M')) value = now.date().month();
        else if (c == QLatin1Char('D')) value = now.date().day();
        else if (c == QLatin1Char('h')) {
            value = now.time().hour();
            if (use12Hour) {
                value = (value + 11) % 12 + 1;
            }
        }
        else if (c == QLatin1Char('m')) value = now.time().minute();
        else if (c == QLatin1Char('s')) value = now.time().second();
        if (value >= 0) {
            int count = 1;
            while (i + count < input.size() && input[i + count] == c)
                ++count;
            int width = count == 1 ? 1 : 2;
            if (c == QLatin1Char('Y')) {
                if (count == 2)
                    value %= 100;
                else
                    width = 4;
            }
            result += QStringLiteral("%1").arg(value, width, 10, QLatin1Char('0'));
            if (c == QLatin1Char('h')) {
                lineHasHour = true;
            } if (c == QLatin1Char('h') || c == QLatin1Char('m') || c == QLatin1Char('s')) {
                timeEnd = result.size();
            }
            i += count - 1;
        } else
            result += c;
    }
    finishTimeLine();
    return result;
}
}
#endif
