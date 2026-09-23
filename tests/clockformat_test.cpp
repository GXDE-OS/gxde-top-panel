#include "frame/util/clockformat.h"
#include <QDebug>
#include <cstdlib>

void check(const QString &actual, const QString &expected)
{
    if (actual != expected) {
        qCritical() << "Clock format:" << actual << "expected:" << expected;
        std::exit(1);
    }
}

int main()
{
    const QDateTime date(QDate(2024, 2, 29), QTime(7, 8, 9));
    const QLocale en(QLocale::English, QLocale::UnitedStates);
    check(ClockFormat::render("Y-M-D ddd h:m:s", date, en), "2024-2-29 Thu 7:8:9");
    check(ClockFormat::render("时间 h:m\\nY年M月D日", date, en), "时间 7:8\n2024年2月29日");
    check(ClockFormat::render(R"(\Y \M \D \ddd \h \m \s)", date, en), "Y M D ddd h m s");
    check(ClockFormat::render(R"(\\n h:m\nY)", date, en), "\\n 7:8\n2024");
    check(ClockFormat::render(R"(Y\\\nM\nD)", date, en), "2024\\\n2 29");
    check(ClockFormat::render("'Y'", date, en), "'2024'");
    check(ClockFormat::render("h:m\\", date, en), "7:8\\");
    check(ClockFormat::render("Y\\nM\\nD", date, en), "2024\n2 29");
    check(ClockFormat::render("Y\r\nM\nD", date, en), "2024\n2 29");
    check(ClockFormat::render("", date, en), "");
    check(ClockFormat::render("ddd", date, QLocale(QLocale::Chinese, QLocale::China)),
          QLocale(QLocale::Chinese, QLocale::China).dayName(4, QLocale::ShortFormat));
    check(ClockFormat::render("h:m:s", QDateTime(QDate(2025, 1, 1), QTime(0, 0, 0)), en), "0:0:0");
    check(ClockFormat::normalize(ClockFormat::normalize("Y\\nM\\nD")), "Y\\nM D");
    check(ClockFormat::render("YYYY MM DD h:ss", date, en), "2024 02 29 7:09");
    check(ClockFormat::render("YY-M-D hh:mm:ss", date, en), "24-2-29 07:08:09");
    check(ClockFormat::render("YYYYMMDD", date, en), "20240229");
    check(ClockFormat::render("hh:mm:ss", QDateTime(QDate(2025, 1, 1), QTime(0, 0, 0)), en), "00:00:00");
    check(ClockFormat::render(R"(\Y\Y\Y\Y YYYY)", date, en), "YYYY 2024");
    for (int hour : {0, 1, 11, 12, 13, 23}) {
        const QDateTime time(QDate(2024, 2, 29), QTime(hour, 8, 9));
        const int expected = hour % 12 == 0 ? 12 : hour % 12;
        check(ClockFormat::render("h hh:mm:ss", time, en, true),
              QString("%1 %2:08:09 %3").arg(expected).arg(expected, 2, 10, QLatin1Char('0')).arg(hour < 12 ? "A.M." : "P.M."));
        check(ClockFormat::render("hh", time, en, false),
              QString("%1").arg(hour, 2, 10, QLatin1Char('0')));
        check(ClockFormat::render(R"(\h)", time, en, true), "h");
    }
    const QDateTime evening(QDate(2024, 2, 29), QTime(23, 17, 9));
    check(ClockFormat::render("hh:mm", evening, en, true), "11:17 P.M.");
    check(ClockFormat::render("hh:mm\\nMM-DD", evening, en, true), "11:17 P.M.\n02-29");
    check(ClockFormat::render("MM-DD\\nhh:mm:ss", evening, en, true), "02-29\n11:17:09 P.M.");
    check(ClockFormat::render("hh:mm MM-DD", evening, en, true), "11:17 P.M. 02-29");
    check(ClockFormat::render("MM-DD", evening, en, true), "02-29");
    return 0;
}
