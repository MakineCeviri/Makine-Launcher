/**
 * @file notificationservice.h
 * @brief Application notification management service
 * @copyright (c) 2026 MakineAI Team
 *
 * Replaces QML-side QtObject+ListModel with a proper C++ QAbstractListModel.
 * Used directly as a ListView model in QML.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QDateTime>

namespace makineai {

struct NotificationItem {
    QString title;
    QString message;
    QString type; // info, update, success, warning, error, translation
    QString time;
    bool read{false};
};

class NotificationService : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        MessageRole,
        TypeRole,
        TimeRole,
        ReadRole
    };

    explicit NotificationService(QObject *parent = nullptr);
    ~NotificationService() override;

    static NotificationService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int unreadCount() const { return m_unreadCount; }
    int count() const { return static_cast<int>(m_items.size()); }

    Q_INVOKABLE void addNotification(const QString& title, const QString& message,
                                      const QString& type = QStringLiteral("info"));
    Q_INVOKABLE void markAsRead(int index);
    Q_INVOKABLE void markAllAsRead();
    Q_INVOKABLE void clear();

signals:
    void unreadCountChanged();
    void countChanged();
    void notificationAdded(const QString& title, const QString& message, const QString& type);

private:
    QList<NotificationItem> m_items;
    int m_unreadCount{0};
};

} // namespace makineai
