#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager();

    void setIcon(const QIcon &icon);
    void show();

signals:
    void showWindowRequested();
    void quitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
};
