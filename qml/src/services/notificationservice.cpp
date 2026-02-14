/**
 * @file notificationservice.cpp
 * @brief Application notification management implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "notificationservice.h"

namespace makineai {

NotificationService::NotificationService(QObject *parent)
    : QAbstractListModel(parent)
{
}

NotificationService::~NotificationService() = default;

NotificationService* NotificationService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    auto *instance = new NotificationService;
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

int NotificationService::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant NotificationService::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto &item = m_items[index.row()];
    switch (role) {
    case TitleRole:   return item.title;
    case MessageRole: return item.message;
    case TypeRole:    return item.type;
    case TimeRole:    return item.time;
    case ReadRole:    return item.read;
    default:          return {};
    }
}

QHash<int, QByteArray> NotificationService::roleNames() const
{
    return {
        {TitleRole,   "title"},
        {MessageRole, "message"},
        {TypeRole,    "type"},
        {TimeRole,    "time"},
        {ReadRole,    "read"}
    };
}

void NotificationService::addNotification(const QString& title, const QString& message,
                                            const QString& type)
{
    NotificationItem item;
    item.title = title;
    item.message = message;
    item.type = type;
    item.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    item.read = false;

    // Insert at the beginning (newest first)
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();

    ++m_unreadCount;
    emit unreadCountChanged();
    emit countChanged();
    emit notificationAdded(title, message, type);
}

void NotificationService::markAsRead(int index)
{
    if (index < 0 || index >= m_items.size())
        return;

    if (!m_items[index].read) {
        m_items[index].read = true;
        m_unreadCount = qMax(0, m_unreadCount - 1);

        const QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {ReadRole});
        emit unreadCountChanged();
    }
}

void NotificationService::markAllAsRead()
{
    if (m_unreadCount == 0)
        return;

    for (auto &item : m_items)
        item.read = true;

    m_unreadCount = 0;

    if (!m_items.isEmpty())
        emit dataChanged(createIndex(0, 0), createIndex(m_items.size() - 1, 0), {ReadRole});
    emit unreadCountChanged();
}

void NotificationService::clear()
{
    if (m_items.isEmpty())
        return;

    beginResetModel();
    m_items.clear();
    endResetModel();

    m_unreadCount = 0;
    emit unreadCountChanged();
    emit countChanged();
}

} // namespace makineai
