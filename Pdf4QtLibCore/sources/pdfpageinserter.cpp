// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#include "pdfpageinserter.h"
#include "pdfaction.h"
#include "pdfcatalog.h"
#include "pdfdocumentbuilder.h"
#include "pdfexception.h"
#include "pdfnumbertreeloader.h"
#include "pdfoutline.h"
#include "pdfsecurityhandler.h"

#include <algorithm>
#include <map>
#include <set>

namespace pdf
{

namespace
{

const PDFDictionary* getCatalogDictionary(const PDFDocument* document)
{
    const PDFDictionary* trailer = document->getTrailerDictionary();
    return trailer ? document->getDictionaryFromObject(trailer->get("Root")) : nullptr;
}

QByteArray getName(const PDFObjectStorage* storage, const PDFDictionary* dictionary, const char* key)
{
    const PDFObject& object = storage->getObject(dictionary->get(key));
    return object.isName() ? object.getString() : QByteArray();
}

PDFObject createDictionaryObject(PDFDictionary dictionary)
{
    return PDFObject::createDictionary(std::make_shared<PDFDictionary>(std::move(dictionary)));
}

PDFObject createRectangleObject(const QRectF& rectangle)
{
    PDFObjectFactory factory;
    factory << rectangle;
    return factory.takeObject();
}

/// Structure tree or "marked" flag in the catalog. Pages and their objects are checked separately.
bool isTaggedDocument(const PDFDocument* document)
{
    const PDFDictionary* catalog = getCatalogDictionary(document);
    if (!catalog)
    {
        return false;
    }
    if (!document->getObject(catalog->get("StructTreeRoot")).isNull())
    {
        return true;
    }
    const PDFDictionary* markInfo = document->getDictionaryFromObject(catalog->get("MarkInfo"));
    return markInfo && PDFDocumentDataLoaderDecorator(document).readBooleanFromDictionary(markInfo, "Marked", false);
}

/// A form exists when the AcroForm lists any field or has an XFA form (an empty AcroForm is not a form).
bool hasForm(const PDFDocument* document, bool* xfa)
{
    *xfa = false;
    const PDFDictionary* form = document->getDictionaryFromObject(document->getCatalog()->getFormObject());
    if (!form)
    {
        return false;
    }
    *xfa = !document->getObject(form->get("XFA")).isNull();
    const PDFObject& fields = document->getObject(form->get("Fields"));
    return *xfa || (fields.isArray() && fields.getArray()->getCount() > 0);
}

/// An outline item that names a page by its number (not by the page object) would lead to a
/// different page after an insertion.
bool hasNumericOutlineDestination(const PDFDocument* document)
{
    bool numeric = false;
    if (QSharedPointer<PDFOutlineItem> root = document->getCatalog()->getOutlineRootPtr())
    {
        root->apply([&numeric](PDFOutlineItem* item)
        {
            const PDFActionGoTo* goTo = dynamic_cast<const PDFActionGoTo*>(item->getAction());
            if (goTo)
            {
                const PDFDestination& destination = goTo->getDestination();
                numeric = numeric || (destination.isValid() && !destination.isNamedDestination() && !destination.getPageReference().isValid());
            }
        });
    }
    return numeric;
}

// Page labels -------------------------------------------------------------------------------------------------------

struct PageLabels
{
    bool present = false;
    bool valid = true;
    std::vector<PDFPageLabel> ranges;
};

PageLabels readPageLabels(const PDFDocument* document)
{
    PageLabels result;
    const PDFDictionary* catalog = getCatalogDictionary(document);
    if (!catalog || document->getObject(catalog->get("PageLabels")).isNull())
    {
        return result;
    }

    result.present = true;
    result.ranges = PDFNumberTreeLoader<PDFPageLabel>::parse(&document->getStorage(), catalog->get("PageLabels"));
    // The ranges must start at the first page and be unique. An entry that is not a label dictionary is parsed as an
    // empty label of page 0 with start 0, so it shows up here as a duplicate or as an invalid start number.
    result.valid = !result.ranges.empty() && result.ranges.front().getPageIndex() == 0;
    for (size_t i = 0; result.valid && i < result.ranges.size(); ++i)
    {
        result.valid = result.ranges[i].getPageStartNumber() >= 1 &&
                       (i == 0 || result.ranges[i].getPageIndex() > result.ranges[i - 1].getPageIndex());
    }
    return result;
}

struct PageLabel
{
    PDFPageLabel::NumberingStyle style = PDFPageLabel::NumberingStyle::DecimalArabic;
    QString prefix;
    PDFInteger number = 1;
};

/// Rewrites the page labels so that every old page keeps its label and the inserted pages get their own range.
/// Ranges are split where needed (new /St values). Nothing happens when the document has no labels.
void updatePageLabels(PDFDocumentBuilder* builder, const PDFDocument* document, PDFInteger insertIndex, PDFInteger insertedCount)
{
    const PageLabels pageLabels = readPageLabels(document);
    if (!pageLabels.present)
    {
        return;
    }

    const PDFInteger oldPageCount = PDFInteger(document->getCatalog()->getPageCount());
    std::vector<PageLabel> labels;
    labels.reserve(size_t(oldPageCount + insertedCount));
    size_t range = 0;
    for (PDFInteger pageIndex = 0; pageIndex < oldPageCount; ++pageIndex)
    {
        while (range + 1 < pageLabels.ranges.size() && pageLabels.ranges[range + 1].getPageIndex() <= pageIndex)
        {
            ++range;
        }
        const PDFPageLabel& definition = pageLabels.ranges[range];
        labels.push_back({ definition.getNumberingStyle(), definition.getPrefix(), definition.getPageStartNumber() + pageIndex - definition.getPageIndex() });
    }
    // Inserted pages are numbered after the label of the page before them ("ii.1", "ii.2"; "0.1" at the beginning),
    // so they do not repeat a label of an existing page.
    QString insertedPrefix = QStringLiteral("0.");
    if (insertIndex > 0)
    {
        const PageLabel& previous = labels[size_t(insertIndex - 1)];
        insertedPrefix = previous.prefix + (previous.style == PDFPageLabel::NumberingStyle::None ? QString() : PDFPageLabel::formatPageNumber(previous.style, previous.number)) + QLatin1Char('.');
    }
    for (PDFInteger i = 0; i < insertedCount; ++i)
    {
        labels.insert(labels.begin() + insertIndex + i, PageLabel{ PDFPageLabel::NumberingStyle::DecimalArabic, insertedPrefix, i + 1 });
    }

    auto styleName = [](PDFPageLabel::NumberingStyle style) -> QByteArray
    {
        switch (style)
        {
            case PDFPageLabel::NumberingStyle::DecimalArabic:    return "D";
            case PDFPageLabel::NumberingStyle::UppercaseRoman:   return "R";
            case PDFPageLabel::NumberingStyle::LowercaseRoman:   return "r";
            case PDFPageLabel::NumberingStyle::UppercaseLetters: return "A";
            case PDFPageLabel::NumberingStyle::LowercaseLetters: return "a";
            default:                                             return QByteArray();
        }
    };

    PDFObjectFactory factory;
    factory.beginDictionary();
    factory.beginDictionaryItem("Nums");
    factory.beginArray();
    for (size_t i = 0; i < labels.size(); ++i)
    {
        const PageLabel& label = labels[i];
        const bool continuesRange = i > 0 &&
                                    labels[i - 1].style == label.style &&
                                    labels[i - 1].prefix == label.prefix &&
                                    (label.style == PDFPageLabel::NumberingStyle::None || labels[i - 1].number + 1 == label.number);
        if (continuesRange)
        {
            continue;
        }

        factory << PDFInteger(i);
        factory.beginDictionary();
        if (label.style != PDFPageLabel::NumberingStyle::None)
        {
            factory.beginDictionaryItem("S");
            factory << WrapName(styleName(label.style));
            factory.endDictionaryItem();
        }
        if (!label.prefix.isEmpty())
        {
            factory.beginDictionaryItem("P");
            factory << PDFObjectFactory::createTextString(label.prefix);
            factory.endDictionaryItem();
        }
        if (label.style != PDFPageLabel::NumberingStyle::None && label.number != 1)
        {
            factory.beginDictionaryItem("St");
            factory << label.number;
            factory.endDictionaryItem();
        }
        factory.endDictionary();
    }
    factory.endArray();
    factory.endDictionaryItem();
    factory.endDictionary();

    const PDFObjectReference catalogReference = builder->getCatalogReference();
    PDFDictionary catalog = *builder->getDictionaryFromObject(builder->getObjectByReference(catalogReference));
    catalog.setEntry(PDFInplaceOrMemoryString("PageLabels"), factory.takeObject());
    builder->setObject(catalogReference, createDictionaryObject(std::move(catalog)));
}

// Page tree ---------------------------------------------------------------------------------------------------------

/// The pages of the target in reading order, as direct children of the page tree root (v8 pattern:
/// a nested tree is flattened first, which writes inherited attributes into the pages).
PDFOperationResult getFlatPages(PDFDocumentBuilder* builder, const PDFDocument* document, std::vector<PDFObjectReference>* pages)
{
    const PDFCatalog* catalog = document->getCatalog();
    const size_t pageCount = catalog->getPageCount();
    *pages = builder->getPages();
    if (pages->size() != pageCount)
    {
        builder->flattenPageTree();
        *pages = builder->getPages();
    }

    bool matches = pages->size() == pageCount;
    for (size_t i = 0; matches && i < pageCount; ++i)
    {
        matches = (*pages)[i] == catalog->getPage(i)->getPageReference();
    }
    if (!matches)
    {
        return PDFPageInserter::tr("The page tree of the current document cannot be changed safely.");
    }
    return true;
}

PDFObjectReference getPageTreeRoot(const PDFDocumentBuilder* builder)
{
    const PDFDictionary* catalog = builder->getDictionaryFromObject(builder->getObjectByReference(builder->getCatalogReference()));
    const PDFObject pages = catalog ? catalog->get("Pages") : PDFObject();
    return pages.isReference() ? pages.getReference() : PDFObjectReference();
}

// External page import ----------------------------------------------------------------------------------------------

struct ImportContext
{
    const PDFDocument* source = nullptr;
    PDFDocumentBuilder* staging = nullptr;              ///< Copy of the source storage; the source stays unchanged
    std::set<PDFObjectReference> selectedPages;
    std::set<PDFObjectReference> annotations;           ///< Annotations on the selected pages
    int droppedLinks = 0;
};

/// Explicit destination to a selected source page, or null. Named destinations and page numbers are resolved
/// against the source document and written as an explicit page reference, which the batch copy maps to the
/// inserted page.
PDFObject mapDestination(const ImportContext& context, const PDFObject& destinationObject)
{
    const PDFObjectStorage* storage = context.staging->getStorage();
    const PDFCatalog* catalog = context.source->getCatalog();
    const PDFObject& object = storage->getObject(destinationObject);

    PDFDestination destination;
    if (object.isName() || object.isString())
    {
        const PDFDestination* named = catalog->getNamedDestination(object.getString());
        if (!named)
        {
            return PDFObject();
        }
        destination = *named;
    }
    else if (object.isArray())
    {
        destination = PDFDestination::parse(storage, object);
    }

    if (!destination.isValid() || destination.isNamedDestination())
    {
        return PDFObject();
    }

    PDFObjectReference page = destination.getPageReference();
    if (!page.isValid())
    {
        const PDFInteger pageIndex = destination.getPageIndex();
        if (pageIndex < 0 || pageIndex >= PDFInteger(catalog->getPageCount()))
        {
            return PDFObject();
        }
        page = catalog->getPage(size_t(pageIndex))->getPageReference();
    }
    if (!context.selectedPages.count(page))
    {
        return PDFObject();
    }

    destination.setPageReference(page);
    PDFObjectFactory factory;
    factory << destination;
    return factory.takeObject();
}

/// Annotation of a selected page, ready to be copied: it belongs to its page, references to annotations of other
/// pages are removed, and navigation to a page that is not inserted is removed (the annotation itself stays).
PDFObject sanitizeAnnotation(ImportContext& context, const PDFObject& annotation, PDFObjectReference page)
{
    const PDFObjectStorage* storage = context.staging->getStorage();
    const PDFDictionary* sourceDictionary = storage->getDictionaryFromObject(annotation);
    if (!sourceDictionary || annotation.isStream())
    {
        return annotation;
    }

    PDFDictionary dictionary = *sourceDictionary;
    dictionary.setEntry(PDFInplaceOrMemoryString("P"), PDFObject::createReference(page));
    for (const char* key : { "IRT", "Popup", "Parent" })
    {
        const PDFObject& related = dictionary.get(key);
        if (related.isReference() && !context.annotations.count(related.getReference()))
        {
            dictionary.removeEntry(key);
        }
    }

    bool navigationDropped = false;
    if (dictionary.hasKey("Dest"))
    {
        PDFObject destination = mapDestination(context, dictionary.get("Dest"));
        if (destination.isNull())
        {
            dictionary.removeEntry("Dest");
            navigationDropped = true;
        }
        else
        {
            dictionary.setEntry(PDFInplaceOrMemoryString("Dest"), std::move(destination));
        }
    }

    // GoTo is resolved here; URI is kept; every other action is left as it is and refused by the graph check.
    if (const PDFDictionary* action = storage->getDictionaryFromObject(dictionary.get("A")))
    {
        if (getName(storage, action, "S") == "GoTo" && !action->hasKey("Next"))
        {
            PDFObject destination = mapDestination(context, action->get("D"));
            if (destination.isNull())
            {
                dictionary.removeEntry("A");
                navigationDropped = true;
            }
            else
            {
                PDFDictionary mappedAction = *action;
                mappedAction.setEntry(PDFInplaceOrMemoryString("D"), std::move(destination));
                dictionary.setEntry(PDFInplaceOrMemoryString("A"), createDictionaryObject(std::move(mappedAction)));
            }
        }
    }

    if (navigationDropped)
    {
        ++context.droppedLinks;
    }
    return createDictionaryObject(std::move(dictionary));
}

/// Writes the effective (possibly inherited) attributes into the selected page and cuts it from the source page
/// tree. Threads (/B) and document parts (/DPart) point back into the source document and are not kept.
void prepareSourcePage(ImportContext& context, const PDFPage* page)
{
    PDFDocumentBuilder* staging = context.staging;
    const PDFObjectReference pageReference = page->getPageReference();
    PDFDictionary dictionary = *staging->getDictionaryFromObject(staging->getObjectByReference(pageReference));

    dictionary.removeEntry("Parent");
    dictionary.removeEntry("B");
    dictionary.removeEntry("DPart");
    dictionary.setEntry(PDFInplaceOrMemoryString("MediaBox"), createRectangleObject(page->getMediaBox()));
    dictionary.setEntry(PDFInplaceOrMemoryString("CropBox"), createRectangleObject(page->getCropBox()));
    PDFInteger angle = 0;
    switch (page->getPageRotation())
    {
        case PageRotation::Rotate90:  angle = 90;  break;
        case PageRotation::Rotate180: angle = 180; break;
        case PageRotation::Rotate270: angle = 270; break;
        default: break;
    }
    dictionary.setEntry(PDFInplaceOrMemoryString("Rotate"), PDFObject::createInteger(angle));
    if (page->getResources().isNull())
    {
        dictionary.removeEntry("Resources");
    }
    else
    {
        dictionary.setEntry(PDFInplaceOrMemoryString("Resources"), PDFObject(page->getResources()));
    }

    const PDFObject& annotations = staging->getObject(dictionary.get("Annots"));
    if (annotations.isArray())
    {
        PDFArray sanitizedAnnotations;
        for (const PDFObject& annotation : *annotations.getArray())
        {
            if (annotation.isReference())
            {
                const PDFObjectReference annotationReference = annotation.getReference();
                staging->setObject(annotationReference, sanitizeAnnotation(context, staging->getObjectByReference(annotationReference), pageReference));
                sanitizedAnnotations.appendItem(annotation);
            }
            else if (annotation.isDictionary())
            {
                sanitizedAnnotations.appendItem(sanitizeAnnotation(context, annotation, pageReference));
            }
        }
        dictionary.setEntry(PDFInplaceOrMemoryString("Annots"), PDFObject::createArray(std::make_shared<PDFArray>(std::move(sanitizedAnnotations))));
    }
    else
    {
        dictionary.removeEntry("Annots");
    }

    staging->setObject(pageReference, createDictionaryObject(std::move(dictionary)));
}

/// Walks everything the prepared pages can reach (the same objects the batch copy would copy) and refuses
/// anything that does not belong to a single page or that v11 does not carry over.
QString validateReachableGraph(const PDFObjectStorage* storage, const std::set<PDFObjectReference>& selectedPages, size_t* objectCount)
{
    struct Item
    {
        PDFObject object;
        QByteArray key;
        PDFObjectReference reference;
    };

    std::set<PDFObjectReference> visited;
    std::vector<Item> stack;
    for (const PDFObjectReference& page : selectedPages)
    {
        stack.push_back({ PDFObject::createReference(page), QByteArray(), PDFObjectReference() });
    }

    auto checkDictionary = [&](const PDFDictionary* dictionary, const QByteArray& parentKey, PDFObjectReference reference) -> QString
    {
        const QByteArray type = getName(storage, dictionary, "Type");
        const QByteArray subtype = getName(storage, dictionary, "Subtype");
        if ((type == "Page" && !selectedPages.count(reference)) || type == "Pages" || type == "Catalog" || type == "Outlines")
        {
            return PDFPageInserter::tr("The selected pages refer to other parts of the PDF in a way that cannot be separated safely.");
        }
        if (dictionary->hasKey("FT") || subtype == "Widget")
        {
            return PDFPageInserter::tr("The selected pages of this PDF contain form fields and cannot be inserted safely yet.");
        }
        if (type == "Sig" || type == "DocTimeStamp" || dictionary->hasKey("ByteRange"))
        {
            return PDFPageInserter::tr("This PDF is digitally signed. Pages of signed PDFs cannot be inserted yet.");
        }
        if (type == "StructTreeRoot" || type == "StructElem" || dictionary->hasKey("StructParent") || dictionary->hasKey("StructParents"))
        {
            return PDFPageInserter::tr("This PDF is a tagged (accessible) PDF. Pages of tagged PDFs cannot be inserted yet.");
        }
        if (type == "OCG" || type == "OCMD" || dictionary->hasKey("OC"))
        {
            return PDFPageInserter::tr("The selected pages use optional content (layers) and cannot be inserted yet.");
        }
        if (dictionary->hasKey("Rect") && (subtype == "Screen" || subtype == "Movie" || subtype == "Sound" || subtype == "RichMedia" || subtype == "3D"))
        {
            return PDFPageInserter::tr("The selected pages contain multimedia or 3D content and cannot be inserted yet.");
        }
        const QByteArray actionType = getName(storage, dictionary, "S");
        const bool isAction = type == "Action" || ((parentKey == "A" || parentKey == "Next") && !actionType.isEmpty());
        if (dictionary->hasKey("AA") || dictionary->hasKey("JS") ||
            (isAction && ((actionType != "URI" && actionType != "GoTo") || dictionary->hasKey("Next"))))
        {
            return PDFPageInserter::tr("The selected pages contain JavaScript, launch or other actions that cannot be inserted.");
        }
        return QString();
    };

    while (!stack.empty())
    {
        Item item = std::move(stack.back());
        stack.pop_back();

        switch (item.object.getType())
        {
            case PDFObject::Type::Reference:
            {
                if (visited.insert(item.object.getReference()).second)
                {
                    stack.push_back({ storage->getObjectByReference(item.object.getReference()), item.key, item.object.getReference() });
                }
                break;
            }

            case PDFObject::Type::Dictionary:
            case PDFObject::Type::Stream:
            {
                const PDFDictionary* dictionary = item.object.isStream() ? item.object.getStream()->getDictionary() : item.object.getDictionary();
                const QString blocker = checkDictionary(dictionary, item.key, item.reference);
                if (!blocker.isEmpty())
                {
                    return blocker;
                }
                for (size_t i = 0; i < dictionary->getCount(); ++i)
                {
                    stack.push_back({ dictionary->getValue(i), dictionary->getKey(i).getString(), PDFObjectReference() });
                }
                break;
            }

            case PDFObject::Type::Array:
            {
                for (const PDFObject& element : *item.object.getArray())
                {
                    stack.push_back({ element, item.key, PDFObjectReference() });
                }
                break;
            }

            default:
                break;
        }
    }

    *objectCount = visited.size();
    return QString();
}

}   // namespace

PDFInteger PDFPageInserter::getInsertIndex(Position position, const std::vector<PDFInteger>& anchorPages, PDFInteger pageCount)
{
    switch (position)
    {
        case Position::Before:
            return anchorPages.empty() ? 0 : std::clamp(anchorPages.front(), PDFInteger(0), pageCount);
        case Position::After:
            return anchorPages.empty() ? pageCount : std::clamp(anchorPages.back() + 1, PDFInteger(0), pageCount);
        case Position::Beginning:
            return 0;
        case Position::End:
            return pageCount;
    }
    return pageCount;
}

std::vector<PDFInteger> PDFPageInserter::parsePageSelection(PDFInteger pageCount, const QString& text, QString* errorMessage)
{
    std::vector<PDFInteger> pages = PDFDocumentMerger::parsePageList(pageCount, text, errorMessage);
    if (!errorMessage->isEmpty())
    {
        return { };
    }
    if (pages.empty())
    {
        *errorMessage = tr("No pages selected.");
        return { };
    }

    std::set<PDFInteger> seen;
    for (const PDFInteger page : pages)
    {
        if (!seen.insert(page).second)
        {
            *errorMessage = tr("Page %1 is selected more than once. Each page can be inserted only once.").arg(page + 1);
            return { };
        }
    }
    return pages;
}

QStringList PDFPageInserter::checkTarget(const PDFDocument* target, bool externalPages)
{
    QStringList blockers;
    if (!target)
    {
        blockers << tr("No document is open.");
        return blockers;
    }

    bool xfa = false;
    hasForm(target, &xfa);
    if (xfa)
    {
        blockers << tr("The current document contains an XFA form. Pages cannot be inserted into it.");
    }
    const PDFSecurityHandler* securityHandler = target->getStorage().getSecurityHandler();
    if (securityHandler && !securityHandler->isAllowed(PDFSecurityHandler::Permission::Assemble))
    {
        blockers << tr("The current document does not allow inserting pages.");
    }
    if (!readPageLabels(target).valid)
    {
        blockers << tr("The page labels of the current document cannot be read reliably. Pages are not inserted, so the labels do not change.");
    }
    if (hasNumericOutlineDestination(target))
    {
        blockers << tr("A bookmark of the current document points to a page number instead of a page. Inserting pages would change where it leads.");
    }
    if (externalPages && isTaggedDocument(target))
    {
        blockers << tr("The current document is a tagged (accessible) PDF. Pages of other PDFs cannot be inserted into it yet; blank pages can.");
    }
    return blockers;
}

QStringList PDFPageInserter::checkSource(const PDFDocumentMerger::Source& source)
{
    QStringList blockers;
    const PDFDocument* document = source.document.data();
    if (!document)
    {
        blockers << tr("The PDF could not be opened.");
        return blockers;
    }

    if (!source.copyAllowed || !source.assembleAllowed)
    {
        blockers << tr("The permissions of this PDF do not allow copying and assembling its pages.");
    }
    bool xfa = false;
    if (hasForm(document, &xfa) || source.hasXfa || !source.fieldNames.empty())
    {
        blockers << tr("The selected pages of this PDF contain form fields and cannot be inserted safely yet.");
    }
    if (source.hasSignature || !document->getObject(document->getCatalog()->getPerms()).isNull())
    {
        blockers << tr("This PDF is digitally signed. Pages of signed PDFs cannot be inserted yet.");
    }
    if (isTaggedDocument(document))
    {
        blockers << tr("This PDF is a tagged (accessible) PDF. Pages of tagged PDFs cannot be inserted yet.");
    }
    return blockers;
}

PDFOperationResult PDFPageInserter::insertBlankPage(const PDFDocument* target,
                                                    PDFInteger insertIndex,
                                                    const QRectF& mediaBox,
                                                    const QRectF& cropBox,
                                                    PageRotation rotation,
                                                    PDFDocumentPointer* result)
{
    result->reset();
    const QStringList blockers = checkTarget(target, false);
    if (!blockers.isEmpty())
    {
        return blockers.join('\n');
    }
    const PDFInteger pageCount = PDFInteger(target->getCatalog()->getPageCount());
    if (insertIndex < 0 || insertIndex > pageCount || !mediaBox.isValid())
    {
        return tr("Invalid insert position or page size.");
    }

    try
    {
        PDFDocumentModifier modifier(target);
        PDFDocumentBuilder* builder = modifier.getBuilder();
        std::vector<PDFObjectReference> pages;
        PDFOperationResult pagesResult = getFlatPages(builder, target, &pages);
        if (!pagesResult)
        {
            return pagesResult;
        }

        // Geometry is written explicitly, so nothing is inherited from the page tree root.
        const PDFObjectReference newPage = builder->appendPage(mediaBox);
        builder->setPageCropBox(newPage, cropBox.isValid() ? cropBox : mediaBox);
        builder->setPageRotation(newPage, rotation);

        pages.insert(pages.begin() + insertIndex, newPage);
        builder->setPages(pages);
        updatePageLabels(builder, target, insertIndex, 1);

        modifier.markReset();
        if (!modifier.finalize())
        {
            return tr("The page could not be inserted.");
        }
        *result = modifier.getDocument();
        return true;
    }
    catch (const PDFException& exception)
    {
        return exception.getMessage();
    }
    catch (const std::exception& exception)
    {
        return tr("The page could not be inserted: %1").arg(QString::fromLocal8Bit(exception.what()));
    }
}

PDFOperationResult PDFPageInserter::insertPages(const PDFDocument* target,
                                                PDFInteger insertIndex,
                                                const PDFDocumentMerger::Source& source,
                                                const std::vector<PDFInteger>& pages,
                                                PDFDocumentPointer* result,
                                                QStringList* warnings)
{
    result->reset();
    warnings->clear();

    QStringList blockers = checkTarget(target, true);
    blockers << checkSource(source);
    if (!blockers.isEmpty())
    {
        blockers.removeDuplicates();
        return blockers.join('\n');
    }

    const PDFDocument* sourceDocument = source.document.data();
    const PDFCatalog* sourceCatalog = sourceDocument->getCatalog();
    const PDFInteger targetPageCount = PDFInteger(target->getCatalog()->getPageCount());
    const PDFInteger sourcePageCount = PDFInteger(sourceCatalog->getPageCount());
    if (insertIndex < 0 || insertIndex > targetPageCount)
    {
        return tr("Invalid insert position.");
    }
    if (pages.empty() || std::set<PDFInteger>(pages.cbegin(), pages.cend()).size() != pages.size() ||
        std::any_of(pages.cbegin(), pages.cend(), [sourcePageCount](PDFInteger page) { return page < 0 || page >= sourcePageCount; }))
    {
        return tr("Invalid page selection.");
    }

    try
    {
        // 1) Prepare the selected pages in a copy of the source.
        PDFDocumentBuilder staging(sourceDocument);
        ImportContext context;
        context.source = sourceDocument;
        context.staging = &staging;

        std::vector<PDFObjectReference> selectedPages;
        selectedPages.reserve(pages.size());
        for (const PDFInteger pageIndex : pages)
        {
            const PDFPage* page = sourceCatalog->getPage(size_t(pageIndex));
            if (!page->getPageReference().isValid() || !staging.getDictionaryFromObject(staging.getObjectByReference(page->getPageReference())))
            {
                return tr("Page %1 of the PDF cannot be read.").arg(pageIndex + 1);
            }
            selectedPages.push_back(page->getPageReference());
            context.selectedPages.insert(page->getPageReference());
            const std::vector<PDFObjectReference>& annotations = page->getAnnotations();
            context.annotations.insert(annotations.cbegin(), annotations.cend());
        }
        for (const PDFInteger pageIndex : pages)
        {
            prepareSourcePage(context, sourceCatalog->getPage(size_t(pageIndex)));
        }

        // 2) Nothing outside the selected pages may come along.
        size_t objectCount = 0;
        const QString graphBlocker = validateReachableGraph(staging.getStorage(), context.selectedPages, &objectCount);
        if (!graphBlocker.isEmpty())
        {
            return graphBlocker;
        }

        // 3) Copy all pages in one batch (links between selected pages are mapped together) and insert them.
        PDFDocumentModifier modifier(target);
        PDFDocumentBuilder* builder = modifier.getBuilder();
        std::vector<PDFObjectReference> targetPages;
        PDFOperationResult pagesResult = getFlatPages(builder, target, &targetPages);
        if (!pagesResult)
        {
            return pagesResult;
        }
        const PDFObjectReference pageTreeRoot = getPageTreeRoot(builder);
        if (!pageTreeRoot.isValid())
        {
            return tr("The page tree of the current document cannot be changed safely.");
        }

        const std::vector<PDFObjectReference> insertedPages =
                PDFDocumentBuilder::createReferencesFromObjects(builder->copyFrom(PDFDocumentBuilder::createObjectsFromReferences(selectedPages), *staging.getStorage(), false));
        for (const PDFObjectReference& insertedPage : insertedPages)
        {
            PDFDictionary page = *builder->getDictionaryFromObject(builder->getObjectByReference(insertedPage));
            page.setEntry(PDFInplaceOrMemoryString("Parent"), PDFObject::createReference(pageTreeRoot));
            builder->setObject(insertedPage, createDictionaryObject(std::move(page)));
        }
        targetPages.insert(targetPages.begin() + insertIndex, insertedPages.cbegin(), insertedPages.cend());
        builder->setPages(targetPages);
        updatePageLabels(builder, target, insertIndex, PDFInteger(insertedPages.size()));

        modifier.markReset();
        if (!modifier.finalize())
        {
            return tr("The pages could not be inserted.");
        }

        if (context.droppedLinks > 0)
        {
            *warnings << tr("%1 link(s) on the inserted pages lead to pages that are not inserted. They stay visible but no longer jump anywhere.").arg(context.droppedLinks);
        }
        const PDFSecurityHandler* targetSecurity = target->getStorage().getSecurityHandler();
        if (source.encrypted && (!targetSecurity || targetSecurity->getMode() == EncryptionMode::None))
        {
            *warnings << tr("The inserted pages will no longer have the encryption of the source PDF.");
        }
        *result = modifier.getDocument();
        return true;
    }
    catch (const PDFException& exception)
    {
        return exception.getMessage();
    }
    catch (const std::exception& exception)
    {
        return tr("The pages could not be inserted: %1").arg(QString::fromLocal8Bit(exception.what()));
    }
}

}   // namespace pdf
