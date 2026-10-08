// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFDOCUMENTMERGER_H
#define PDFDOCUMENTMERGER_H

#include "pdfdocument.h"
#include "pdfutils.h"

#include <QCoreApplication>
#include <QStringList>

#include <atomic>
#include <functional>
#include <set>
#include <vector>

namespace pdf
{

/// "Merge PDFs" engine: several source PDFs (each with its own page list) are
/// assembled into one NEW file. Sources are never modified. Page copying is done
/// by PDFDocumentManipulator; this class adds the ordered page-range parser,
/// the source safety checks (permissions, encryption, signatures, form names)
/// and the atomic write.
class PDF4QTLIBCORESHARED_EXPORT PDFDocumentMerger
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFDocumentMerger)

public:
    /// One PDF that takes part in a merge, together with what was found out about it.
    struct Source
    {
        QString fileName;               ///< File on disk (also used to protect it from being overwritten); can be empty
        QString displayName;            ///< Name shown in messages
        PDFDocumentPointer document;
        PDFInteger pageCount = 0;
        bool encrypted = false;         ///< Source has a security handler; the output will NOT keep it
        bool copyAllowed = true;        ///< Permission "copy content" is granted
        bool assembleAllowed = true;    ///< Permission "assemble document" is granted
        bool hasSignature = false;      ///< Source contains at least one digital signature
        bool hasXfa = false;            ///< Source has an XFA form (not carried over)
        bool hasNamedDestinations = false; ///< Links that use named destinations do not work after the merge
        std::set<QString> fieldNames;   ///< Fully qualified AcroForm field names
    };

    enum class LoadStatus
    {
        OK,
        Failed,
        WrongPassword,
        Cancelled
    };

    struct LoadResult
    {
        LoadStatus status = LoadStatus::Failed;
        QString errorMessage;
        Source source;
    };

    /// One merge list row: a source and the zero based pages taken from it, in output order.
    struct Entry
    {
        Source source;
        std::vector<PDFInteger> pages;
    };

    /// Result of checking the whole merge list before merging.
    struct Report
    {
        QStringList blockers;   ///< Merge must not run
        QStringList warnings;   ///< User must confirm (data not carried over / not valid after merge)
    };

    /// Maximum number of password attempts for one encrypted source.
    static constexpr int MAX_PASSWORD_ATTEMPTS = 3;

    /// Reads a PDF from disk with the normal PDF reader. If a password is needed,
    /// \p passwordCallback is used (same contract as PDFDocumentReader); it is asked
    /// at most MAX_PASSWORD_ATTEMPTS times.
    static LoadResult loadSource(const QString& fileName, const std::function<QString(bool*)>& passwordCallback);

    /// Inspects an already open document (for example the document shown in the viewer).
    static Source createSource(const QString& fileName, const QString& displayName, PDFDocumentPointer document);

    /// Parses a page list such as "all", "1-3,8,10-12" or "3,1". Unlike
    /// PDFClosedIntervalSet::parsePageSelection the order and repetitions the user typed
    /// are kept ("3,1" gives pages 3 then 1). Result is zero based; empty text means all pages.
    static std::vector<PDFInteger> parsePageList(PDFInteger pageCount, const QString& text, QString* errorMessage);

    /// Checks the merge list. Never changes anything.
    static Report analyze(const std::vector<Entry>& entries);

    /// Assembles the entries and writes the result to \p destination. The destination is replaced
    /// only after the complete PDF was written, so failure or cancel leaves it untouched.
    /// The destination must not be one of the sources. \p cancel is checked between the phases
    /// (it cannot interrupt a single phase that is already running).
    /// \returns true, or an error message. A cancel is reported with \p cancelled set to true.
    static PDFOperationResult mergeToFile(const std::vector<Entry>& entries,
                                          const QString& destination,
                                          const std::atomic_bool* cancel,
                                          bool* cancelled);

    /// Assembles the entries into \p document in memory; nothing is written to disk. Entries that
    /// share the same PDFDocument object share one source, so a document listed in several rows
    /// (for example "pages before", "inserted pages", "pages after") is copied once.
    /// \returns true, or an error message.
    static PDFOperationResult mergeToDocument(const std::vector<Entry>& entries, PDFDocument* document);

private:
    static PDFOperationResult mergeToFileImpl(const std::vector<Entry>& entries,
                                              const QString& destination,
                                              const std::function<bool()>& isCancelled);
};

}   // namespace pdf

#endif // PDFDOCUMENTMERGER_H
