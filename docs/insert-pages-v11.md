# Insert pages (v11)

Editor only. Viewer has no entry, and the controller refuses the change (`insertBlankPageAt` / `insertPagesAt` return false when there is no Undo manager).

## Entries

- Edit → Insert Blank Page... / Insert Pages from PDF... (anchor: the current page).
- Thumbnail menu → Insert Blank Page... / Insert Pages from PDF... (anchor: the selected thumbnails).
- Position: before page N, after page N, beginning, end. Before uses the first selected page and After uses the last selected page. The combo box shows the actual page number, and a label shows the page numbers that the new pages get.
- Blank size: same as the anchor page (MediaBox, CropBox, Rotate) or A4. The blank page has no content and no resources. MediaBox, CropBox and Rotate are written into the page, so it inherits nothing from the page tree root.
- Pages from PDF: one source file, page list such as `3,1` or `2-5`. The order typed is kept (v9 `parsePageList`). A page given twice (`3,1,3`) is an error and is not silently removed.

## Flow

`PDFProgramController` (dialog `PDFInsertPagesDialog`) → `pdf::PDFPageInserter` (Core) → new document → `onDocumentModified(Reset | PreserveUndoRedo | PreserveView)` → one Undo step. Then the view goes to the first inserted page, and the inserted thumbnails are selected (the old selection is cleared).

The engine always works on copies. Failure, refusal, cancel or wrong password leave the open document, the Undo history, the thumbnail selection and the current page unchanged. `onDocumentModified` is called only after success.

### Restricted page import (`PDFPageInserter::insertPages`)

1. The source is loaded with v9 `PDFDocumentMerger::loadSource` (password callback, max 3 attempts). The source must allow both CopyContent and Assemble.
2. Source checks: AcroForm with fields, XFA, signature (SigFlags / signature field / Perms), tagged (StructTreeRoot or MarkInfo /Marked true). If any check fails, the source is refused.
3. Staging copy of the source (`PDFDocumentBuilder(source)`). For each selected page, the inherited MediaBox, CropBox, Resources and Rotate are written into the page. Then `/Parent`, `/B` (threads) and `/DPart` are removed.
4. Annotations on the selected pages: `/P` is set to their page. `/IRT`, `/Popup` and `/Parent` that point outside the selected pages are removed. Links are rewritten (see below).
5. Graph check: all objects that the selected pages can reach are walked, which is the same set that `copyFrom` copies. Any of these refuses the import: an unselected Page, Pages, Catalog or Outlines; a Widget or `/FT`; Sig, DocTimeStamp or `/ByteRange`; StructTreeRoot, StructElem, `/StructParent(s)`; OCG, OCMD or `/OC`; Screen, Movie, Sound, RichMedia or 3D annotations; `/AA`, `/JS`; any action other than URI or GoTo; any action chain (`/Next`).
6. One `copyFrom` batch with all selected pages, so links between them map together. `/Parent` is set to the target page tree root, page references are inserted, page labels are updated, and `finalize()`.

`copyFrom`, the manipulator and the AcroForm, structure and encryption cores are not changed.

## Policies

| Topic | Policy |
|---|---|
| Links | URI is kept. GoTo to a selected page (explicit, named, or page number) becomes an explicit reference to the inserted page. GoTo to a page that is not inserted, or to an unknown name: the destination or action is removed and the annotation and its appearance stay. One warning before publishing ("N link(s) ..."), with Cancel. The source Names tree and bookmarks are never imported. |
| Target outline | Bookmarks point to page objects, so they keep their page. A bookmark with a page-number destination: every insertion is refused. |
| Page labels | Every old page keeps its label (ranges are split, and `/St` is written). Inserted pages get `<previous label>.<n>` (`ii.1`, `A-5.2`), or `0.<n>` at the beginning. A document without labels gets none. Labels that cannot be read reliably (first range not at page 1, duplicates, `/St` < 1): insertion is refused. |
| Forms | A source form is refused (no flatten, rename or removal). The target AcroForm is kept unchanged. A target with XFA: every insertion is refused. |
| Signatures | A signed source is refused. A signed target follows the v6 policy: a full rewrite, so the old signature is not kept valid. |
| Tagged | A tagged source is refused. Into a tagged target: blank pages are allowed (the structure tree is kept, no `removeStructureTree`), and external pages are refused. |
| Optional content | Refused only when the selected pages depend on OCG or OCMD. Unrelated `/OCProperties` in the source do not block, and OCProperties are never merged. |
| Encryption | The inserted pages do not keep the source encryption. When the target is not encrypted, a warning is shown first. An encrypted target keeps its security handler (Save, Save As and reopen stay encrypted). A target without the Assemble permission: insertion is refused. |

## Limits

- Pages that share a `/Resources` dictionary, for example inherited from the source root, bring every object in that dictionary along. This is the same as v8 flatten. Unused XObjects are not pruned.
- Undo keeps whole document snapshots. Measured: inserting 400 of 500 scanned pages adds about 13 MB of private memory, and Undo and Redo add nothing.
- The import is synchronous with a wait cursor (100 pages: about 1 ms, 400 scanned pages: about 15–20 ms). No worker or cancel was needed.

## Tests

- `UnitTestsInsertPages` (Core, 32 functions, written and read back): positions, geometry, nested tree, flat root attributes, page order and duplicates, inherited Rotate/CropBox, graph isolation (one page of 5 brings no other page, content, bead or thread), JPEG 2000 bytes, embedded font program, annotations with appearance, links, form / rogue widget / XFA, signed, tagged, optional content, active content, encrypted source, wrong password / cancel, CopyContent / Assemble denied, encrypted target roundtrip, target form, outline, XFA / tagged target, page labels, failure atomicity, source lifetime, measurements.
- `UnitTestsViewer`: `insertPagesViewerIsReadOnly`, `insertPagesEditorEntries`, `insertPagesEditorBlankWorkflow`, `insertPagesEditorFromPdfWorkflow`, `insertPagesEditorFailureAndWarnings`, `insertPagesEditorUndoMemory`, `insertPagesEditorTranslations`.
