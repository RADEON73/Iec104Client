#include "appwindow.h"

#include <algorithm>
#include <qcoreapplication.h>
#include <qfile.h>
#include <qmainwindow.h>
#include <qregularexpression.h>
#include <qsettings.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qstringliteral.h>
#include <qtabwidget.h>
#include <qwidget.h>
#include "mainwindow.h"

namespace
{
    constexpr auto kCurrentDirIniFile = "/qtester104.ini";
    constexpr auto kConfDirIniFile = "../conf/qtester104.ini";
}

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent), mTabs(new QTabWidget(this)), mIniPath(resolveIniPath())
{
    setCentralWidget(mTabs);
    setWindowTitle(QStringLiteral("QTester104 IEC60870-5-104"));

    QSettings settings(mIniPath, QSettings::IniFormat);
    const QStringList rtuSections = discoverRtuSections(settings);

    for (int index = 0; index < rtuSections.size(); ++index) {
        const QString& section = rtuSections.at(index);
        auto* client = new MainWindow(mIniPath, section, index == 0, mTabs);
        mTabs->addTab(client, tabTitleForSection(settings, section));
    }
}

AppWindow::~AppWindow() = default;

QString AppWindow::resolveIniPath() const
{
    QString iniPath = QCoreApplication::applicationDirPath() + kCurrentDirIniFile;
    if (!QFile(iniPath).exists()) {
        iniPath = kConfDirIniFile;
    }

    if (QCoreApplication::arguments().count() > 1) {
        iniPath = QCoreApplication::arguments().at(1);
    }

    return iniPath;
}

QStringList AppWindow::discoverRtuSections(QSettings& settings) const
{
    QStringList rtuSections;
    const QRegularExpression sectionPattern(QStringLiteral("^RTU(\\d+)$"));

    for (const QString& group : settings.childGroups()) {
        if (sectionPattern.match(group).hasMatch()) {
            rtuSections.append(group);
        }
    }

    if (rtuSections.isEmpty()) {
        rtuSections.append(QStringLiteral("RTU1"));
    }

    std::sort(rtuSections.begin(), rtuSections.end(),
        [&sectionPattern](const QString& left, const QString& right) {
            const auto leftMatch = sectionPattern.match(left);
            const auto rightMatch = sectionPattern.match(right);
            return leftMatch.captured(1).toInt() < rightMatch.captured(1).toInt();
        });

    return rtuSections;
}

QString AppWindow::tabTitleForSection(QSettings& settings,
    const QString& section) const
{
    const QString ip = settings.value(section + "/IP_ADDRESS", "").toString().trimmed();
    if (ip.isEmpty()) {
        return section;
    }
    return section + " - " + ip;
}
