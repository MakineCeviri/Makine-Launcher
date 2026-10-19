// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// Framing between the launcher and makine-plugin-host.exe: a uint32
// little-endian length, then that many bytes of UTF-8 JSON (one JSON-RPC 2.0
// message). tools/plugin-host/plugin_host.cpp is the other end.

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <optional>

namespace makine::pluginwire {

inline constexpr quint32 kMaxFrame = 16u * 1024 * 1024;

inline QByteArray encode(const QJsonObject& message)
{
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray frame(4, Qt::Uninitialized);
    qToLittleEndian<quint32>(static_cast<quint32>(body.size()), frame.data());
    return frame + body;
}

/// Cuts a byte stream into messages.
class Decoder {
public:
    void append(const QByteArray& bytes) { m_buffer += bytes; }

    /// The next complete message, or nothing until more bytes arrive. A frame
    /// that is too large or not a JSON object breaks the stream for good.
    std::optional<QJsonObject> next()
    {
        if (m_failed || m_buffer.size() < 4) return std::nullopt;
        const quint32 len = qFromLittleEndian<quint32>(m_buffer.constData());
        if (len > kMaxFrame) {
            m_failed = true;
            return std::nullopt;
        }
        if (m_buffer.size() < 4 + qsizetype(len)) return std::nullopt;
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(m_buffer.mid(4, len), &error);
        m_buffer.remove(0, 4 + qsizetype(len));
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            m_failed = true;
            return std::nullopt;
        }
        return doc.object();
    }

    bool failed() const { return m_failed; }

private:
    QByteArray m_buffer;
    bool m_failed = false;
};

} // namespace makine::pluginwire
