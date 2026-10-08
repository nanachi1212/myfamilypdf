// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfdocumentmerger.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentmanipulator.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfform.h"
#include "pdfoptimizer.h"
#include "pdfsecurityhandler.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <map>
#include <set>

namespace pdf
{

namespace
{

/// Copy of \p object where references from \p removed are replaced by null (or, in arrays, dropped).
PDFObject dropReferences(const PDFObject& object, const std::set<PDFObjectReference>& removed, bool eraseFromArrays)
{
    switch (object.getType())
    {
        case PDFObject::Type::Reference:
            return removed.count(object.getReference()) ? PDFObject() : object;

        case PDFObject::Type::Stream:
        {
            const PDFStream* stream = object.getStream();
            PDFDictionary dictionary = *stream->getDictionary();
            for (size_t i = 0, count = dictionary.getCount(); i < count; ++i)
            {
                dictionary.setEntry(dictionary.getKey(i), dropReferences(dictionary.getValue(i), removed, eraseFromArrays));
            }
            return PDFObject::createStream(std::make_shared<PDFStream>(std::move(dictionary), QByteArray(*stream->getContent())));
        }

        case PDFObject::Type::Dictionary:
        {
            PDFDictionary dictionary = *object.getDictionary();
            for (size_t i = 0, count = dictionary.getCount(); i < count; ++i)
            {
                dictionary.setEntry(dictionary.getKey(i), dropReferences(dictionary.getValue(i), removed, eraseFromArrays));
            }
            return PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(dictionary)));
        }

        case PDFObject::Type::Array:
        {
            PDFArray array;
            for (const PDFObject& item : *object.getArray())
            {
                if (!(eraseFromArrays && item.isReference() && removed.count(item.getReference())))
                {
                    array.appendItem(dropReferences(item, removed, eraseFromArrays));
                }
            }
            return PDFObject::createArray(std::make_shared<PDFArray>(std::move(array)));
        }

        default:
            return object;
    }
}

/// The page copier keeps every page that a kept page (or one of its form fields) points to: a link to a page
/// that was left out pulls that whole page, with its content, into the file as an unused object. This removes
/// such pages: form fields that only live on left-out pages are dropped, the remaining references to
/// left-out pages become null (a link to a page that is not there anymore), and unused objects are removed.
void pruneExcludedPages(PDFDocument* document)
{
    PDFDocumentBuilder builder(document);
    const std::vector<PDFObjectReference> pages = builder.getPages();
    const std::set<PDFObjectReference> keptPages(pages.cbegin(), pages.cend());

    std::set<PDFObjectReference> excludedPages;
    const PDFObjectStorage::PDFObjects& objects = document->getStorage().getObjects();
    for (size_t objectNumber = 1; objectNumber < objects.size(); ++objectNumber)
    {
        const PDFObjectReference reference(PDFInteger(objectNumber), objects[objectNumber].generation);
        const PDFDictionary* dictionary = document->getStorage().getDictionaryFromObject(objects[objectNumber].object);
        if (dictionary && !keptPages.count(reference))
        {
            const PDFObject type = dictionary->get("Type");
            if (type.isName() && type.getString() == "Page")
            {
                excludedPages.insert(reference);
            }
        }
    }
    if (excludedPages.empty())
    {
        return;
    }

    // Form fields whose widgets are all on left-out pages leave the AcroForm.
    const PDFObject formObject = document->getCatalog()->getFormObject();
    if (formObject.isReference())
    {
        std::set<PDFObjectReference> removedFields;
        const PDFForm form = PDFForm::parse(document, formObject);
        for (const PDFFormFieldPointer& field : form.getFormFields())
        {
            size_t widgetCount = 0;
            size_t excludedWidgetCount = 0;
            field->apply([&](const PDFFormField* child)
            {
                for (const PDFFormWidget& widget : child->getWidgets())
                {
                    ++widgetCount;
                    excludedWidgetCount += excludedPages.count(widget.getPage()) ? 1 : 0;
                }
            });
            if (widgetCount > 0 && widgetCount == excludedWidgetCount)
            {
                removedFields.insert(field->getSelfReference());
            }
        }
        if (!removedFields.empty())
        {
            builder.setObject(formObject.getReference(), dropReferences(builder.getObjectByReference(formObject.getReference()), removedFields, true));
        }
    }

    for (size_t objectNumber = 1; objectNumber < objects.size(); ++objectNumber)
    {
        const PDFObjectReference reference(PDFInteger(objectNumber), objects[objectNumber].generation);
        if (!excludedPages.count(reference) && !builder.getObjectByReference(reference).isNull())
        {
            builder.setObject(reference, dropReferences(builder.getObjectByReference(reference), excludedPages, false));
        }
    }

    PDFDocument pruned = builder.build();
    PDFOptimizer optimizer(PDFOptimizer::OptimizationFlags(PDFOptimizer::RemoveUnusedObjects | PDFOptimizer::ShrinkObjectStorage), nullptr);
    optimizer.setDocument(&pruned);
    optimizer.optimize();
    *document = optimizer.takeOptimizedDocument();
}

/// Form-level entries that a plain dictionary merge gets wrong: the XFA form of a source cannot describe the merged
/// document (the user was warned that it is not carried over), and when one source needs its field appearances
/// generated, a later source's "NeedAppearances false" must not hide those field values.
void finishMergedForm(PDFDocument* document, bool needAppearances)
{
    const PDFObject formObject = document->getCatalog()->getFormObject();
    const PDFDictionary* formDictionary = document->getStorage().getDictionaryFromObject(formObject);
    if (!formObject.isReference() || !formDictionary)
    {
        return;
    }
    const bool hasNeedAppearances = PDFDocumentDataLoaderDecorator(document).readBooleanFromDictionary(formDictionary, "NeedAppearances", false);
    if (!formDictionary->hasKey("XFA") && (!needAppearances || hasNeedAppearances))
    {
        return;
    }

    PDFDictionary form = *formDictionary;
    form.removeEntry("XFA");
    if (needAppearances)
    {
        form.setEntry(PDFInplaceOrMemoryString("NeedAppearances"), PDFObject::createBool(true));
    }
    PDFDocumentBuilder builder(document);
    builder.setObject(formObject.getReference(), PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(form))));
    *document = builder.build();
}

}   // namespace

PDFDocumentMerger::Source PDFDocumentMerger::createSource(const QString& fileName, const QString& displayName, PDFDocumentPointer document)
{
    Source source;
    source.fileName = fileName;
    source.displayName = displayName;
    source.document = document;
    if (!document)
    {
        return source;
    }

    source.pageCount = PDFInteger(document->getCatalog()->getPageCount());

    // The security handler is the existing permission API (it already answers "yes" for the owner).
    const PDFSecurityHandler* securityHandler = document->getStorage().getSecurityHandler();
    if (securityHandler)
    {
        source.encrypted = securityHandler->getMode() != EncryptionMode::None;
        source.copyAllowed = securityHandler->isAllowed(PDFSecurityHandler::Permission::CopyContent);
        source.assembleAllowed = securityHandler->isAllowed(PDFSecurityHandler::Permission::Assemble);
    }

    const PDFForm form = PDFForm::parse(document.data(), document->getCatalog()->getFormObject());
    source.hasXfa = form.isXFAForm();
    source.hasNamedDestinations = !document->getCatalog()->getNamedDestinations().empty();
    source.hasSignature = form.getSignatureFlags().testFlag(PDFForm::SignatureExists);
    form.apply([&source](const PDFFormField* field)
    {
        if (field->getFieldType() == PDFFormField::FieldType::Signature)
        {
            const auto* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field);
            if (signatureField && signatureField->getSignature().getType() != PDFSignature::Type::Invalid)
            {
                source.hasSignature = true;
            }
        }
        if (!field->getChildFields().empty())
        {
            return;     // only terminal fields carry values
        }
        const QString& name = field->getName(PDFFormField::FullyQualified);
        if (!name.isEmpty())
        {
            source.fieldNames.insert(name);
        }
    });

    return source;
}

PDFDocumentMerger::LoadResult PDFDocumentMerger::loadSource(const QString& fileName, const std::function<QString(bool*)>& passwordCallback)
{
    LoadResult result;
    int attempts = 0;
    auto queryPassword = [&attempts, &passwordCallback](bool* ok)
    {
        *ok = false;
        if (!passwordCallback || attempts >= MAX_PASSWORD_ATTEMPTS)
        {
            return QString();
        }
        QString password = passwordCallback(ok);
        if (*ok)
        {
            ++attempts;
        }
        return password;
    };

    PDFDocumentReader reader(nullptr, queryPassword, true, false);
    PDFDocument document = reader.readFromFile(fileName);
    switch (reader.getReadingResult())
    {
        case PDFDocumentReader::Result::OK:
            result.status = LoadStatus::OK;
            result.source = createSource(fileName,
                                         QFileInfo(fileName).fileName(),
                                         PDFDocumentPointer(new PDFDocument(std::move(document))));
            break;

        case PDFDocumentReader::Result::Cancelled:
            // Password asked, a wrong one entered (or attempts exhausted) and then the prompt ended.
            result.status = attempts > 0 ? LoadStatus::WrongPassword : LoadStatus::Cancelled;
            if (result.status == LoadStatus::WrongPassword)
            {
                result.errorMessage = tr("The password is not correct.");
            }
            break;

        case PDFDocumentReader::Result::Failed:
            result.status = LoadStatus::Failed;
            result.errorMessage = reader.getErrorMessage();
            break;
    }

    return result;
}

std::vector<PDFInteger> PDFDocumentMerger::parsePageList(PDFInteger pageCount, const QString& text, QString* errorMessage)
{
    errorMessage->clear();
    const QString trimmed = text.trimmed();
    std::vector<PDFInteger> pages;
    if (trimmed.isEmpty() || trimmed.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0)
    {
        pages.reserve(size_t(qMax<PDFInteger>(pageCount, 0)));
        for (PDFInteger i = 0; i < pageCount; ++i)
        {
            pages.push_back(i);
        }
        return pages;
    }

    static const QRegularExpression partPattern(QStringLiteral("^([0-9]+)(?:\\s*-\\s*([0-9]+))?$"));
    for (const QString& part : trimmed.split(',', Qt::KeepEmptyParts))
    {
        const auto match = partPattern.match(part.trimmed());
        if (!match.hasMatch())
        {
            *errorMessage = tr("Invalid page selection '%1'. Use all, page numbers or ranges such as 1-3,8,10-12.").arg(part.trimmed());
            return { };
        }

        bool lowOK = false;
        bool highOK = true;
        const PDFInteger low = match.captured(1).toLongLong(&lowOK);
        const PDFInteger high = match.captured(2).isEmpty() ? low : match.captured(2).toLongLong(&highOK);
        if (!lowOK || !highOK || low < 1 || high < 1 || low > pageCount || high > pageCount)
        {
            *errorMessage = tr("Page numbers must be between 1 and %1.").arg(pageCount);
            return { };
        }
        if (low > high)
        {
            *errorMessage = tr("Range '%1' is reversed. Put the smaller page number first.").arg(part.trimmed());
            return { };
        }

        for (PDFInteger page = low; page <= high; ++page)
        {
            pages.push_back(page - 1);
        }
    }

    return pages;
}

PDFDocumentMerger::Report PDFDocumentMerger::analyze(const std::vector<Entry>& entries)
{
    Report report;
    if (entries.empty())
    {
        report.blockers << tr("Add at least one PDF file.");
        return report;
    }

    QStringList encrypted;
    QStringList signedSources;
    QStringList xfaSources;
    QStringList namedDestinationSources;
    for (const Entry& entry : entries)
    {
        const Source& source = entry.source;
        if (!source.document || source.pageCount < 1)
        {
            report.blockers << tr("%1: the document has no pages.").arg(source.displayName);
            continue;
        }
        if (entry.pages.empty())
        {
            report.blockers << tr("%1: no pages selected.").arg(source.displayName);
        }
        if (!source.copyAllowed || !source.assembleAllowed)
        {
            report.blockers << tr("%1: the document permissions do not allow its pages to be copied into another PDF.").arg(source.displayName);
        }
        if (source.encrypted && !encrypted.contains(source.displayName))
        {
            encrypted << source.displayName;
        }
        if (source.hasSignature && !signedSources.contains(source.displayName))
        {
            signedSources << source.displayName;
        }
        if (source.hasNamedDestinations && !namedDestinationSources.contains(source.displayName))
        {
            namedDestinationSources << source.displayName;
        }
        if (source.hasXfa && !xfaSources.contains(source.displayName))
        {
            xfaSources << source.displayName;
        }
    }

    if (!encrypted.isEmpty())
    {
        report.warnings << tr("Encrypted source (%1): the merged PDF is not encrypted and does not keep the password or permissions.").arg(encrypted.join(QLatin1String(", ")));
    }
    if (!signedSources.isEmpty())
    {
        report.warnings << tr("Digitally signed source (%1): a signature cannot stay valid in a merged PDF. It will be shown as invalid or broken.").arg(signedSources.join(QLatin1String(", ")));
    }
    if (!xfaSources.isEmpty())
    {
        report.warnings << tr("XFA form source (%1): the dynamic XFA form is not carried over.").arg(xfaSources.join(QLatin1String(", ")));
    }

    if (!namedDestinationSources.isEmpty())
    {
        report.warnings << tr("Named destinations (%1): internal links that use a named destination do not work in the merged PDF. Bookmarks and links to a page keep working.").arg(namedDestinationSources.join(QLatin1String(", ")));
    }

    // Same field name in two list rows: the merged form would hold equal names that act as one field.
    std::map<QString, int> nameUse;
    for (const Entry& entry : entries)
    {
        for (const QString& name : entry.source.fieldNames)
        {
            ++nameUse[name];
        }
    }
    QStringList duplicates;
    for (const auto& item : nameUse)
    {
        if (item.second > 1)
        {
            duplicates << item.first;
        }
    }
    if (!duplicates.isEmpty())
    {
        constexpr int maxListed = 5;
        QString names = duplicates.mid(0, maxListed).join(QLatin1String(", "));
        if (duplicates.size() > maxListed)
        {
            names += tr(" (+%1 more)").arg(duplicates.size() - maxListed);
        }
        report.warnings << tr("Form fields with the same name exist in more than one source (%1). In the merged PDF they may share one value or behave unpredictably.").arg(names);
    }

    return report;
}

PDFOperationResult PDFDocumentMerger::mergeToFile(const std::vector<Entry>& entries,
                                                  const QString& destination,
                                                  const std::atomic_bool* cancel,
                                                  bool* cancelled)
{
    auto isCancelled = [cancel, cancelled]()
    {
        const bool result = cancel && cancel->load();
        if (cancelled)
        {
            *cancelled = result;
        }
        return result;
    };
    if (cancelled)
    {
        *cancelled = false;
    }

    if (entries.empty())
    {
        return tr("Add at least one PDF file.");
    }
    if (destination.isEmpty())
    {
        return tr("No output file selected.");
    }

    // The sources must stay exactly as they are.
    const QFileInfo destinationInfo(destination);
    for (const Entry& entry : entries)
    {
        if (!entry.source.fileName.isEmpty() && QFileInfo(entry.source.fileName) == destinationInfo)
        {
            return tr("The output file must be different from the source PDFs (%1).").arg(entry.source.displayName);
        }
    }

    if (isCancelled())
    {
        return tr("Cancelled.");
    }

    // Upstream object code can throw on damaged input (also non-PDF exceptions); the merge must report it, not
    // let it reach the caller's thread or event loop.
    try
    {
        return mergeToFileImpl(entries, destination, isCancelled);
    }
    catch (const PDFException& exception)
    {
        return exception.getMessage();
    }
    catch (const std::exception& exception)
    {
        return tr("The PDFs could not be merged (%1).").arg(QString::fromLocal8Bit(exception.what()));
    }
}

PDFOperationResult PDFDocumentMerger::mergeToDocument(const std::vector<Entry>& entries, PDFDocument* document)
{
    PDFDocumentManipulator manipulator;
    manipulator.setOutlineMode(PDFDocumentManipulator::OutlineMode::Join);
    manipulator.setAttachMergedCatalogObjects(true);
    PDFDocumentManipulator::AssembledPages assembledPages;
    bool needAppearances = false;
    // One source per distinct document object. A file opened twice is two objects and stays two
    // independent sources; the same object in several rows (page insertion) is copied once.
    std::map<const PDFDocument*, int> documentIndices;
    for (const Entry& entry : entries)
    {
        if (!entry.source.document)
        {
            return tr("Invalid document.");
        }
        const PDFDocument* source = entry.source.document.data();
        auto it = documentIndices.find(source);
        if (it == documentIndices.end())
        {
            it = documentIndices.emplace(source, int(documentIndices.size())).first;
            if (const PDFDictionary* form = source->getStorage().getDictionaryFromObject(source->getCatalog()->getFormObject()))
            {
                needAppearances = needAppearances || PDFDocumentDataLoaderDecorator(source).readBooleanFromDictionary(form, "NeedAppearances", false);
            }
            manipulator.addDocument(it->second, source);
            manipulator.setDocumentCaption(it->second, entry.source.displayName);
        }
        const int documentIndex = it->second;
        const PDFDocumentManipulator::AssembledPages allPages = PDFDocumentManipulator::createAllDocumentPages(documentIndex, source);
        for (const PDFInteger pageIndex : entry.pages)
        {
            if (pageIndex < 0 || pageIndex >= PDFInteger(allPages.size()))
            {
                return tr("Missing page (%1) in a document.").arg(pageIndex + 1);
            }
            assembledPages.push_back(allPages[size_t(pageIndex)]);
        }
    }

    PDFOperationResult result = manipulator.assemble(assembledPages);
    if (!result)
    {
        return result;
    }

    *document = manipulator.takeAssembledDocument();
    pruneExcludedPages(document);
    finishMergedForm(document, needAppearances);
    return true;
}

PDFOperationResult PDFDocumentMerger::mergeToFileImpl(const std::vector<Entry>& entries,
                                                      const QString& destination,
                                                      const std::function<bool()>& isCancelled)
{
    PDFDocument assembled;
    PDFOperationResult result = mergeToDocument(entries, &assembled);
    if (!result)
    {
        return result;
    }
    if (isCancelled())
    {
        return tr("Cancelled.");
    }

    // Commit only a complete PDF. QSaveFile's direct-write fallback stays disabled.
    PDFDocumentWriter writer(nullptr);
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly))
    {
        return output.errorString();
    }
    result = writer.write(&output, &assembled);
    if (result && isCancelled())
    {
        result = tr("Cancelled.");
    }
    if (result && !output.commit())
    {
        result = output.errorString();
    }
    if (!result)
    {
        output.cancelWriting();
    }
    return result;
}

}   // namespace pdf
