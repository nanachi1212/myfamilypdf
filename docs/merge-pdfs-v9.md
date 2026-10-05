# Merge PDFs v9

File -> Merge PDFs... (Viewer and Editor). Several PDFs are put in an order, the pages of
each one are chosen, and the result is written as ONE NEW PDF. The open document and the
listed files are never changed, Undo/Redo is not involved, and the open document is not closed.

## Behaviour

- Dialog (`Pdf4QtLibGui/pdfmergepdfsdialog.*`): Add Files, Add Open Document (the document as it is in
  memory, including unsaved edits), Remove, Move Up/Down, drag and drop reordering of the rows,
  per-row page count and page range (editable), Output file + Browse. Merge asks for the output name
  (existing file: asks before replacing; the output must not be a listed file).
- Output order is exactly: list order, then the order of each row's page range. `3,1` gives page 3
  then page 1; `all` (or the localized "All pages", or empty) is every page; a page may be repeated
  (it is copied). The ordered parser is `PDFDocumentMerger::parsePageList` - the existing
  `PDFClosedIntervalSet::parsePageSelection` sorts and removes duplicates (right for print/extract)
  so it is not used here. The same file may be listed twice.
- Encrypted sources use the normal reader and its password callback (3 attempts, then "wrong
  password"; cancel skips the file). Output is never encrypted; no output password in this version.
- Progress/cancel: `PDFDocumentManipulator` and `PDFDocumentWriter` have no progress or cancel hooks, so
  the merge runs in a worker (`QtConcurrent`), the dialog shows a busy bar and Cancel. Cancel is checked
  between phases (before assemble, after assemble, after write, before commit); it cannot stop a
  phase that is already running. Cancel and failure leave the destination untouched.
- Atomic output: `QSaveFile` (same pattern as Extract Pages) - the destination is replaced only after the
  complete PDF was written; the source/destination equality check uses `QFileInfo` equality.
- After a successful merge a message offers "Open Merged PDF": with a document open this opens a new
  tab (`openDocumentInNewTabRequested`), nothing is closed.

## Backend reuse and the changes made to it

- Pages are copied by the existing multi-document `PDFDocumentManipulator::assemble()` (deep copy of
  indirect objects; content, fonts, images, JPEG 2000 data, annotations are copied as they are,
  nothing is rasterized or re-encoded). No second object importer exists.
- `PDFDocumentMerger` (`Pdf4QtLibCore/sources/pdfdocumentmerger.*`) is the engine used by the dialog and the tests.
- Three small additions to the manipulator, all opt-in or cosmetic (PageMaster behaviour is unchanged):
  - `setDocumentCaption()`: outline entry per source uses the file name instead of "Document 0".
  - `setAttachMergedCatalogObjects(true)`: `finalizeMergedObjects()` existed but was never called, so a
    multi-document merge silently dropped the AcroForm and OCProperties of all sources. With the flag they
    are attached (form fields and widgets keep their values, optional content layers keep working).
    The merged Names tree is deliberately NOT attached (see below).
  - Before a source's AcroForm / OCProperties is added to the merged one, entries that refer to an array or a dictionary
    (`/Fields 12 0 R`, `/DR 13 0 R`, `/OCGs`, `/D /ON` ...) are made direct (`dereferenceForMerge`). The dictionary merge
    only sees direct objects: an indirect `/Fields` replaced the fields merged before, or (direct array first, indirect
    second) threw a non-PDF exception. This also applies to PageMaster, which runs the same merge.
- `finishMergedForm()` (in `pdfdocumentmerger.cpp`): `/XFA` is removed from the merged AcroForm (the XFA form of one source
  cannot describe the merged file; the user is warned that it is not carried over), and `/NeedAppearances true` is kept
  when any source has it (a later source's `false` would hide field values that have no appearance stream).
- `mergeToFile` turns any exception from the object code into an error message (the destination is untouched), so
  damaged input cannot end the program from the worker thread.
- `pruneExcludedPages()` (in `pdfdocumentmerger.cpp`, runs on the assembled document only when needed): the
  page copier also keeps every page that a kept page links to, or that carries a form field, as an unused
  object - i.e. pages the user left out, with their content, stayed in the file. The pass removes form
  fields that live only on left-out pages, turns the remaining references to left-out pages into null
  (a dead link) and removes the unused objects. (Extract Pages has the same upstream behaviour; it was
  not changed.)

## Upstream limitations, classified

A - accepted and documented, no warning:
- Tagged PDF structure tree, document actions (OpenAction/AA), article threads are removed in a multi-document merge.
- Links to a page that was left out go nowhere (the page entry of the destination becomes null; viewers ignore such a
  link). Links between pages of different sources are re-targeted correctly.
- A page listed twice: the second one is a full copy. Its links to other pages go nowhere, and its form widgets are not
  listed in the AcroForm `/Fields` (FamilyPDF still shows them as a separate field with the same name; other viewers may
  show only the appearance). The first copy keeps working links and fields.
- A field whose widgets are split between kept and left-out pages keeps its left-out widgets as annotations without a page.
- Form-level defaults are not reconciled: when sources disagree, the last source's `/DA`, `/SigFlags`, equally named `/DR`
  resources and optional content `/BaseState` win.
- Outline: one entry per source (file name; page range for partial sources); the source's own bookmarks
  are kept below it only when all its pages are used. Bookmarks of partial sources are dropped.
- The generated document parts (`/DPartRoot`) come from the existing manipulator.
- Not checked: page labels, viewer preferences, embedded files, metadata of the sources.

B - warning before merge (user must confirm):
- Encrypted source: output is not encrypted and has no password/permissions.
- Digitally signed source: the byte range of a signature can never match the new file; it is shown as
  invalid (checked with the OpenSSL verifier: no valid signature in the output). No re-signing.
- Form fields with the same name in more than one row: both fields are kept with their own value and widget,
  but names are not changed (the manipulator has no safe rename), so viewers may treat them as one field.
- XFA form source: the XFA form is not carried over (`/XFA` is removed; the AcroForm fields stay). (No real XFA fixture;
  the warning itself is not exercised by a test, the removal is.)
- Named destinations: `PDFDocumentBuilder::mergeNames` writes the name tree keys as names instead of strings
  (an invalid tree) and its destinations drag all pages of the sources into the file, so the Names tree is
  not attached. Links that use a named destination do not work; bookmarks and page links do.

C - merge blocked:
- Source cannot be opened (wrong password, damaged file) - it is not added to the list.
- Source permissions do not grant BOTH "copy content" and "assemble document" (PDFSecurityHandler::isAllowed; the owner
  password grants everything). The output is unencrypted, so a source that forbids copying must not be merged.
- No pages selected, invalid range, output equal to a source file.

Needed core changes (deep copy / reference remapping, AcroForm merge core, writer, security, signature) were NOT made.

## Tests

- `UnitTestsMergePdfs` (`UnitTests/tst_mergepdfstest.cpp`, engine, real files, reopened): parser (order, repeat,
  errors), 2 and 3 sources, ranges/custom order/repeats, mixed sizes + rotation + CropBox, annotations and /P,
  AcroForm different and duplicate names, excluded pages/fields/links leave nothing behind, bookmarks, named
  destination warning, links across reordered sources, optional content, indirect `/Fields` `/DR` `/OCGs` in every
  direct/indirect order with `/XFA` removed and `NeedAppearances` kept, page `3,1,3` as three page objects, JPEG 2000 byte-identical, encrypted source with
  password / wrong password / cancel / permission denied, signed source (warning + no valid signature), overwrite,
  source == destination, cancel and failure atomicity (no stray temp files), sources unchanged, timings.
- `UnitTestsViewer`: `mergePdfsEntriesAreAvailable`, `mergePdfsDialogWorkflow` (menu, Add Open Document, ranges, move,
  remove, output, Save, open document untouched), `mergePdfsOutputOrderAndTextLayerAfterReopen` (output opened in FamilyPDF, text
  layer per page, Open Merged PDF), `mergePdfsBlocksAndWarns`, `mergePdfsCancelLeavesNoPartialFile`,
  `mergePdfsTranslations` (zh-TW and zh-CN).
- Not covered by an automated test: dragging rows with the mouse (QTreeWidget InternalMove; Move Up/Down are tested) and a visual
  check of the merged files in Edge (see below).
- `FAMILYPDF_MERGE_ARTIFACT_DIR=<dir> UnitTestsMergePdfs` also writes real sample PDFs (text, form fields, check box,
  annotations with appearances, rotation) and merged results for a look in other viewers.

## Timings (local Release, offscreen, small synthetic fixtures)

2 small PDFs (6 pages): 4 ms. 10 PDFs x 20 pages (200 pages): ~20 ms. 5 PDFs x 200 pages (1,000 pages): ~78 ms,
output 835 KB. Same 200-page PDF listed 5 times: ~77 ms. JPEG 2000 scan page x 500 (500 pages): load 186 ms,
merge+write 102 ms (identical objects are merged, output 302 KB). No bottleneck, no optimizer work.

## Edge check

NOT_TESTED in this change: the sample files from `FAMILYPDF_MERGE_ARTIFACT_DIR` (`smoke-merged.pdf`, `smoke-merged-order.pdf`,
`smoke-merged-duplicate-names.pdf`) were generated but not looked at in Edge (an automated Edge capture was not possible without
capturing the whole desktop). Open them in Edge to compare page order, form field values, check box and annotation appearance.
