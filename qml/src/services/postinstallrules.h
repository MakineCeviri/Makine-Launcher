// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// What must the user still do once a patch is installed?
//
// The field report that produced this file: "Far Cry 6 yama kurulu görünüyor
// ama oyun hâlâ İngilizce." The patch had installed correctly. The catalogue
// note told the user to pick Turkish from the in-game language menu, and Far
// Cry 6 has no Turkish entry — the pack writes a Dunia patch archive over an
// existing slot. The instruction was unfollowable, and nothing in the pipeline
// could tell, because the note is free prose nobody validates.
//
// Two rules live here, and both answer that question from something checkable
// rather than from prose:
//
//   languageSlotFromPaths  — the files an install actually wrote are on disk
//                            and usually name their language. "Data/
//                            Localization/English.pak" means the translation
//                            went into the English slot, whatever the note
//                            claims. Derived, not asserted.
//
//   noteRequiresUserAction — a note is either an instruction ("dili İngilizce
//                            yapın") or a description of the patch ("Unity
//                            resources.assets dosyasını değiştirir"). Only the
//                            first is worth interrupting someone for. 61 of
//                            the catalogue's 237 packages carry a note; 32 ask
//                            for something. Showing all 61 as a modal trains
//                            people to dismiss the ones that matter.
//
// Lives outside gameservice.cpp for the usual reason: the failure mode is a
// confidently wrong sentence shown to a user, so reading the code is not a way
// to know it is right. scripts/catalog_notes_audit.py carries the same tables
// and applies them to the whole catalogue offline — keep the two in step.

#include <QChar>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace makine::postinstall {

// Language tokens as the stores and engines actually spell them, mapped to the
// Turkish name we show. Unreal writes ".int" for International (= English),
// FromSoftware writes "engus"/"jpnjp", Avalanche writes "_pol", Larian writes
// "English.pak". Every entry below was read off a real package in the catalogue.
inline const QHash<QString, QString>& languageTable()
{
    static const QHash<QString, QString> table = {
        {QStringLiteral("english"),   QStringLiteral("İngilizce")},
        {QStringLiteral("eng"),       QStringLiteral("İngilizce")},
        {QStringLiteral("en"),        QStringLiteral("İngilizce")},
        {QStringLiteral("int"),       QStringLiteral("İngilizce")},   // Unreal
        {QStringLiteral("engus"),     QStringLiteral("İngilizce")},   // FromSoftware
        {QStringLiteral("french"),    QStringLiteral("Fransızca")},
        {QStringLiteral("francais"),  QStringLiteral("Fransızca")},
        {QStringLiteral("fra"),       QStringLiteral("Fransızca")},
        {QStringLiteral("fr"),        QStringLiteral("Fransızca")},
        {QStringLiteral("frafr"),     QStringLiteral("Fransızca")},
        {QStringLiteral("german"),    QStringLiteral("Almanca")},
        {QStringLiteral("deutsch"),   QStringLiteral("Almanca")},
        {QStringLiteral("deu"),       QStringLiteral("Almanca")},
        {QStringLiteral("ger"),       QStringLiteral("Almanca")},
        {QStringLiteral("de"),        QStringLiteral("Almanca")},
        {QStringLiteral("gerde"),     QStringLiteral("Almanca")},
        {QStringLiteral("spanish"),   QStringLiteral("İspanyolca")},
        {QStringLiteral("espanol"),   QStringLiteral("İspanyolca")},
        {QStringLiteral("esn"),       QStringLiteral("İspanyolca")},
        {QStringLiteral("esp"),       QStringLiteral("İspanyolca")},
        {QStringLiteral("es"),        QStringLiteral("İspanyolca")},
        {QStringLiteral("spaes"),     QStringLiteral("İspanyolca")},
        {QStringLiteral("italian"),   QStringLiteral("İtalyanca")},
        {QStringLiteral("italiano"),  QStringLiteral("İtalyanca")},
        {QStringLiteral("ita"),       QStringLiteral("İtalyanca")},
        {QStringLiteral("it"),        QStringLiteral("İtalyanca")},
        {QStringLiteral("itait"),     QStringLiteral("İtalyanca")},
        {QStringLiteral("polish"),    QStringLiteral("Lehçe")},
        {QStringLiteral("polski"),    QStringLiteral("Lehçe")},
        {QStringLiteral("pol"),       QStringLiteral("Lehçe")},
        {QStringLiteral("pl"),        QStringLiteral("Lehçe")},
        {QStringLiteral("polpl"),     QStringLiteral("Lehçe")},
        {QStringLiteral("russian"),   QStringLiteral("Rusça")},
        {QStringLiteral("rus"),       QStringLiteral("Rusça")},
        {QStringLiteral("ru"),        QStringLiteral("Rusça")},
        {QStringLiteral("rusru"),     QStringLiteral("Rusça")},
        {QStringLiteral("portuguese"),QStringLiteral("Portekizce")},
        {QStringLiteral("ptb"),       QStringLiteral("Portekizce")},
        {QStringLiteral("pt"),        QStringLiteral("Portekizce")},
        {QStringLiteral("ptbbr"),     QStringLiteral("Portekizce")},
        {QStringLiteral("japanese"),  QStringLiteral("Japonca")},
        {QStringLiteral("jpn"),       QStringLiteral("Japonca")},
        {QStringLiteral("ja"),        QStringLiteral("Japonca")},
        {QStringLiteral("jpnjp"),     QStringLiteral("Japonca")},
        {QStringLiteral("korean"),    QStringLiteral("Korece")},
        {QStringLiteral("kor"),       QStringLiteral("Korece")},
        {QStringLiteral("ko"),        QStringLiteral("Korece")},
        {QStringLiteral("korkr"),     QStringLiteral("Korece")},
        {QStringLiteral("chinese"),   QStringLiteral("Çince")},
        {QStringLiteral("chs"),       QStringLiteral("Çince")},
        {QStringLiteral("cht"),       QStringLiteral("Çince")},
        {QStringLiteral("zh"),        QStringLiteral("Çince")},
        {QStringLiteral("zhocn"),     QStringLiteral("Çince")},
        {QStringLiteral("turkish"),   QStringLiteral("Türkçe")},
        {QStringLiteral("türkçe"),    QStringLiteral("Türkçe")},
        {QStringLiteral("turkce"),    QStringLiteral("Türkçe")},
        {QStringLiteral("tur"),       QStringLiteral("Türkçe")},
        {QStringLiteral("tr"),        QStringLiteral("Türkçe")},
    };
    return table;
}

namespace detail {

// "tr_TR", "en-US", "pt_BR" all name the language in their first half.
inline QString collapseRegionTag(const QString& token)
{
    if (token.size() != 5) return token;
    if (token[2] != QLatin1Char('-') && token[2] != QLatin1Char('_')) return token;
    for (const int i : {0, 1, 3, 4})
        if (!token[i].isLetter()) return token;
    return token.left(2);
}

inline void takeToken(const QString& raw, QSet<QString>& out)
{
    const QString token = collapseRegionTag(raw.trimmed().toLower());
    if (token.isEmpty()) return;
    const auto it = languageTable().constFind(token);
    if (it != languageTable().cend()) out.insert(it.value());
}

// Every language a single path names. A whole path segment, the extension, the
// filename stem, or a word inside the stem — but never a substring, or
// "Englishtown.dat" and a game whose own name contains a language would match.
inline QSet<QString> languagesInPath(const QString& path)
{
    QSet<QString> found;
    const QStringList segments =
        QString(path).replace(QLatin1Char('\\'), QLatin1Char('/'))
                     .split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) return found;

    for (int i = 0; i < segments.size() - 1; ++i)
        takeToken(segments[i], found);                  // Localization/INT/...

    const QString& name = segments.last();
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot > 0 ? name.left(dot) : name;
    const QString ext  = dot > 0 ? name.mid(dot + 1) : QString();

    takeToken(ext, found);                              // .int  .FR  .EN
    takeToken(stem, found);                             // English.pak  tr.po

    // Words inside the stem: "global_pol", "01 - New House_Türkçe",
    // "mad_max_polish". Split by hand — <regex> is broken on MinGW 13.1 and a
    // QRegularExpression here would be a full parser for four separators.
    QString word;
    for (const QChar ch : stem) {
        if (ch == QLatin1Char('_') || ch == QLatin1Char('-')
            || ch == QLatin1Char('.') || ch == QLatin1Char(' ')) {
            takeToken(word, found);
            word.clear();
        } else {
            word.append(ch);
        }
    }
    takeToken(word, found);
    return found;
}

} // namespace detail

// Which in-game language did these files land in? Empty when undecidable.
//
// Empty is a real answer and a common one: Far Cry 6 writes
// "data_win32/patch.fat", a hash-keyed Dunia archive that names no language at
// all. Guessing there would reproduce the bug this file exists to stop, so the
// caller falls back to the catalogue note instead.
//
// The majority is counted against EVERY file, not against the ones carrying a
// language token, and that distinction is the whole rule. The Witcher's pack
// holds 4712 files of which 901 are named "it_amm_001.uti" — "it" is Aurora's
// prefix for "item", not Italian. Counted among tagged files that reads as 100%
// Italian; counted among all files it is 19% and rightly loses. A real language
// slot dominates its package: Thief 200/212, Curse of the Dead Gods 7/8,
// Mad Max 386/386, Alan Wake 2 2/2 (all measured against the live catalogue).
inline QString languageSlotFromPaths(const QStringList& paths)
{
    QHash<QString, int> counts;
    for (const QString& path : paths) {
        const QSet<QString> langs = detail::languagesInPath(path);
        if (langs.size() == 1) counts[*langs.cbegin()] += 1;  // two languages → no vote
    }
    if (counts.isEmpty() || paths.isEmpty()) return {};

    QString best;
    int bestCount = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > bestCount) { bestCount = it.value(); best = it.key(); }
    }
    return bestCount * 2 > paths.size() ? best : QString{};
}

// Does this note ask the user to DO something, or just describe the patch?
//
// The imperative suffix is the whole distinction and it must be part of the
// match. "İngilizce metin dosyasını Türkçe ile değiştirir" reports what the
// patch does; "dili İngilizce olarak değiştirin" asks the user to act. A
// pattern loose enough to accept "değiştir" cannot tell them apart.
inline bool noteRequiresUserAction(const QString& note)
{
    static const QStringList markers = {
        QStringLiteral("unutmay"),          // ...seçmeyi unutmayın
        QStringLiteral("seçmey"),   QStringLiteral("secmey"),
        QStringLiteral("seçin"),    QStringLiteral("secin"),
        QStringLiteral("seçiniz"),  QStringLiteral("seciniz"),
        QStringLiteral("seçilmeli"),QStringLiteral("secilmeli"),
        QStringLiteral("değiştirin"),   QStringLiteral("degistirin"),
        QStringLiteral("değiştiriniz"), QStringLiteral("degistiriniz"),
        QStringLiteral("yapın"),    QStringLiteral("yapin"),
        QStringLiteral("yapınız"),  QStringLiteral("yapiniz"),
        QStringLiteral("yapmanız"), QStringLiteral("yapmaniz"),
        QStringLiteral("açın"),     QStringLiteral("acin"),
        QStringLiteral("açmay"),    QStringLiteral("acmay"),
        QStringLiteral("kapatıp"),  QStringLiteral("kapatip"),
        QStringLiteral("kapatın"),  QStringLiteral("kapatin"),
        QStringLiteral("etkinleştirin"), QStringLiteral("etkinlestirin"),
        QStringLiteral("etkinleştirmey"),QStringLiteral("etkinlestirmey"),
        QStringLiteral("kopyalayın"),QStringLiteral("kopyalayin"),
        QStringLiteral("ekleyin"),  QStringLiteral("kurun"),
        QStringLiteral("çalıştırın"),QStringLiteral("calistirin"),
        QStringLiteral("girin"),    QStringLiteral("indirin"),
        QStringLiteral("bırakın"),  QStringLiteral("birakin"),
        QStringLiteral("işaretleyin"), QStringLiteral("isaretleyin"),
        QStringLiteral("gerekir"),  QStringLiteral("gereklidir"),
        QStringLiteral("gerekmekte"), QStringLiteral("gerekiyor"),
    };
    for (const QString& marker : markers) {
        if (note.contains(marker, Qt::CaseInsensitive)) return true;
    }
    return false;
}

// Should a successful install interrupt the user at all?
//
// Two independent reasons, and nothing else:
//
//   * the translation landed in a FOREIGN language slot. Curse of the Dead
//     Gods writes the French files, Alan Wake 2 the English ones — the game
//     shows nothing until it is switched to that language, and in both cases
//     the catalogue note fails to say so. This is the Far Cry 6 shape and it
//     has to be surfaced whether or not anyone wrote it down.
//
//   * the note itself asks for something.
//
// A Turkish slot is not a reason. Gone Home adds real Turkish files and
// Pacific Drive ships a UE4 override: the default already works, and a modal
// there is the noise that teaches people to dismiss the modal that matters.
inline bool shouldShowAfterInstall(const QString& note, const QString& languageSlot)
{
    if (!languageSlot.isEmpty() && languageSlot != QStringLiteral("Türkçe"))
        return true;
    return noteRequiresUserAction(note);
}

} // namespace makine::postinstall
