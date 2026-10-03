# Annotation / markup workflow v5

Editor text selection now offers Highlight, Underline and Strikeout in its context menu. Existing creation tools remain available. Select All is split into one standard annotation per page and committed as one undoable change; ordinary dragging is still confined to one page. New selection marks use their selected text as initial PDF Contents, which can be edited in the existing properties dialog.

Add Comment starts the existing point-picking sticky-note tool, now with multiline input. The annotation sidebar shows page, type and a shortened Contents value in page order. Its independent, case-insensitive literal filter searches full Contents (including Chinese); clicking a row scrolls the annotation into view. Right-click uses the existing properties and delete actions. Edits and Undo/Redo refresh the list; document reset clears the filter and list. A document with no annotations still exposes the sidebar and its empty state.

The shared annotation manager explicitly enables editing only for the Editor, checks document permissions and annotation lock/read-only flags, and blocks drag/drop, copy, edit and delete in Viewer. Viewer retains annotation rendering, popup reading, filtering and navigation.

## PDF correctness

No changes to the annotation serializer, writer, object ownership, appearance engine, or Undo/Redo core. New marks use PDFTextSelectionPainter quadrilaterals, PDFDocumentBuilder creation methods and updateAnnotationAppearanceStreams. All edits continue through PDFDocumentModifier and PDFProgramController::onDocumentModified. No sidecar, flattening, rasterization, or text-layer changes.

## Validation (Windows Release, 2026-10-04)

- UnitTestsViewer: annotationMarkupWorkflow (three types), annotationNoteWorkflow; targeted searchExperience (Editor/Viewer), thumbnailSelectionAndPageManagement and readingPositionRestoresZoomAndClamps: 10 passed, no failures/skips including setup/cleanup.
- Tests verify exact partial-selection quad coordinates, multiline and multiple/page marks, per-page Select All, standard subtype and AP/N streams, unchanged page Contents, Save, Save As, reopen, note property edits/deletion, Undo/Redo, list refresh/filter/jump, close/switch, malformed/unsupported entries and Viewer mutation boundaries.
- UnitTestsDocumentEdit: 23 passed, no failures/skips including setup/cleanup.
- annotationListLargeDocument: 500 pages and 100 notes, list refresh 8 ms; filter under 1 ms on the local machine. Timing is informational, not a universal latency guarantee.
- Release build passed. dist/FamilyPDF binaries, PDF plugins and translations updated; existing Qt/FFmpeg runtime retained.
- Native UIA smoke on dist/FamilyPDF with clean PATH: Editor creation, list filtering and page-12 jump, multiline note, properties edit, deletion, Undo/Redo, Save and Ctrl+F. Viewer reopen, visible marks/note icon, readonly menu, filtering and page jump passed. Loaded module checks found no developer Qt/vcpkg paths.
- Installed Microsoft Edge PDF reader opened the saved file and displayed highlights, underlines, strikeouts and the note icon.

Local smoke screenshots/logs are under build/annotation-v5 (ignored). CI runs the existing full suites; no local duplicate full-suite run was needed.
