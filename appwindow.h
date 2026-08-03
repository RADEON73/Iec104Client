#pragma once

#include <qmainwindow.h>
#include <qobjectdefs.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qwidget.h>

class QSettings;
class QTabWidget;

class AppWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit AppWindow(QWidget* parent = nullptr);
  ~AppWindow() override;

 private:
  QString resolveIniPath() const;
  QStringList discoverRtuSections(QSettings& settings) const;
  QString tabTitleForSection(QSettings& settings, const QString& section) const;

  QTabWidget* mTabs;
  QString mIniPath;
};