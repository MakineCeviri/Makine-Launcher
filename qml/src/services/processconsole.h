// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// What a patch's own program prints, made readable on the install page.
//
// A "run" step used to start its program hidden, with stdin closed, and kept
// the output only for one log line when the exit code was non-zero. To the
// user the program was a black box: a .bat that asked a question got EOF and
// went on, one that failed looked the same as one still working. The game page
// now shows the output as it arrives and passes typed answers back.
//
// Header-only, Qt Core only: the byte handling is tested without constructing
// the package manager.

#include <QByteArray>
#include <QString>
#include <QStringDecoder>
#include <QStringView>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace makine::processconsole {

// The text in `pending` that can be shown now. What stays behind is the start
// of a UTF-8 character the next read completes; `final` takes everything.
//
// The program picks the encoding, not us: Python (we ask it for UTF-8) and a
// .bat after "chcp 65001" write UTF-8, cmd and older tools the OEM code page
// (857 on Turkish Windows). A read that is valid UTF-8 is taken as UTF-8,
// anything else as OEM; ASCII reads the same either way.
inline QString takeText(QByteArray& pending, bool final = false)
{
    qsizetype keep = 0;
    if (!final) {
        for (qsizetype i = pending.size() - 1, seen = 1; i >= 0 && seen <= 4; --i, ++seen) {
            const auto c = static_cast<uchar>(pending.at(i));
            if ((c & 0xC0) == 0x80)
                continue;  // continuation byte: its lead is further back
            const int need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (need > seen)
                keep = seen;
            break;
        }
    }
    const QByteArray bytes = pending.first(pending.size() - keep);
    pending.remove(0, bytes.size());
    if (bytes.isEmpty())
        return {};

    // Stateless: a sequence left incomplete by the last read is an error here,
    // not state carried into a next call that never comes.
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = utf8.decode(bytes);
    if (!utf8.hasError())
        return text;
#ifdef Q_OS_WIN
    const int n = MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(),
                                      static_cast<int>(bytes.size()), nullptr, 0);
    QString oem(n, Qt::Uninitialized);
    MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(), static_cast<int>(bytes.size()),
                        reinterpret_cast<wchar_t*>(oem.data()), n);
    return oem;
#else
    return QString::fromLocal8Bit(bytes);
#endif
}

// Append program output to `log` the way a console shows it, keeping the
// newest `cap` characters (cut at a line start).
//
// A bare "\r" returns to the start of the line: progress counters rewrite one
// line instead of filling the view. The return is kept at the end of `log`
// until the next character says what it was, because a "\r\n" can arrive split
// across two reads. Callers showing `log` drop a trailing "\r".
inline void appendTo(QString& log, QStringView text, qsizetype cap)
{
    for (const QChar c : text) {
        if (c == u'\n') {
            if (log.endsWith(u'\r'))
                log.chop(1);
            log.append(c);
        } else if (c == u'\r') {
            if (!log.endsWith(u'\r'))
                log.append(c);
        } else {
            if (log.endsWith(u'\r'))
                log.truncate(log.lastIndexOf(u'\n') + 1);
            log.append(c);
        }
    }
    if (log.size() > cap) {
        const qsizetype nl = log.indexOf(u'\n', log.size() - cap);
        log.remove(0, nl < 0 ? log.size() - cap : nl + 1);
    }
}

// Whether the last line of `log` only asks for a key to go on: cmd's "pause"
// ("Press any key to continue . . .", "Devam etmek için bir tuşa basın . . .")
// and the same said in words ("Press Enter to continue", "Devam etmek için
// Enter'a basın"). The install page answers these itself; nobody needs to
// press Enter for a pause. A question that wants an answer — E/H, a path, a
// number, "seçimini yazıp Enter'a bas" — is not one and stays the user's.
inline bool waitsForKey(QStringView log)
{
    QStringView line = log;
    while (!line.isEmpty() && (line.back() == u'\n' || line.back() == u'\r' || line.back() == u' '))
        line.chop(1);
    const qsizetype nl = line.lastIndexOf(u'\n');
    if (nl >= 0)
        line = line.sliced(nl + 1);
    const QString l = line.toString().toLower();
    const auto has = [&l](const char16_t* what) { return l.contains(QStringView(what)); };
    if (has(u"any key") || has(u"tuşa bas") || has(u"tusa bas"))
        return true;
    return has(u"enter") && (has(u"continue") || has(u"devam") || has(u"exit") || has(u"close")
                             || has(u"çık") || has(u"kapat"));
}

} // namespace makine::processconsole
