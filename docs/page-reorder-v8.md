# Thumbnail drag-and-drop page reordering v8

The Editor can reorder pages by dragging thumbnails in the sidebar. The Viewer
keeps its read-only thumbnails (no drag, no drop).

## Behaviour

- Drag one thumbnail, or a Ctrl / Shift selection of several, and drop it between
  two thumbnails. The pages keep their relative order, are taken out of their old
  place and inserted at the drop position. Forward and backward moves both work:
  `1 2 3 4 5 6`, move `2 3` behind `5` gives `1 4 5 2 3 6`.
- Dropping inside the own area of the selection changes nothing and creates no
  Undo step.
- After the move the moved pages stay selected, and the page that was being read
  is still shown (it may have a new page number).
- Undo and Redo use the existing `PDFUndoRedoManager`. One drop is one step.
- The sidebar stays on the Thumbnails page after a move and after Undo / Redo, so
  the next drag can start at once. (`PDFSidebarWidget::setDocument` used to jump to
  the first page, the outline, after every edit that resets the document; edits that
  keep the undo history, such as deleting pages, now keep the current sidebar page
  too.)

## Where the drop lands

`PDFThumbnailsListView` (`Pdf4QtLibGui/pdfthumbnailslistview.*`) is the sidebar's
`QListView`. Whether the standard model drag and drop gives a reliable insertion row
in IconMode was checked first with a throw-away probe (Qt 6.9.1, offscreen,
`QListView` in IconMode with static movement, `DragDrop` mode, a model with drag and
drop enabled): synthetic drops at seven positions (left, centre and right of an item,
top and bottom edge, below the items, right of the items) never reached
`QAbstractItemModel::dropMimeData`, so the standard path gave no usable row. The
view therefore resolves the position itself from `visualRect()` of every item
(`PDFPageReorder::computeInsertionPoint`), and the layout is kept as it is
(IconMode, not ListMode):

| Cursor | Insertion |
|---|---|
| Over an item, left / upper half | before it |
| Over an item, right / lower half | after it |
| Beside the items of a row, in a gap | before the first item whose centre is right of the cursor |
| Right of the last item of a row | after that item |
| Between two rows | before the first item of the lower row |
| Above all items | start of the document |
| Below all items | end of the document |

A layout with several columns decides on the horizontal axis, a single column on
the vertical axis. Nothing depends on a pixel size, so any thumbnail size and
sidebar width works. The view shows an insertion line next to the item the
decision was made on, and scrolls while the cursor is near the top or bottom edge.

The view accepts drops only while a drag started by itself is running. File drops
over the sidebar therefore still reach the main window.

## Data flow

```
PDFThumbnailsListView  pagesDropped(movedPages, insertionRow)
  -> PDFSidebarWidget  computes newPageOrder (PDFPageReorder::computeNewPageOrder)
                       emits reorderPagesRequested(newPageOrder)
  -> PDFProgramController::reorderPages
       validates: complete permutation of 0..n-1 (size, range, duplicates, missing)
       identical order: no-op
       PDFDocumentModifier -> builder->getPages() -> reorder -> builder->setPages()
       modifier.markReset(); onDocumentModified(Reset | PreserveUndoRedo)
```

`newPageOrder[i]` is the old index of the page that becomes page `i`. The view and
the model never change the document.

Only the list of page references in the page tree changes. Page objects are not
copied, serialized or re-imported, so contents, annotations, AcroForm widgets,
rotation, resources and the text layer stay exactly as they were. If the document
has a nested page tree it is flattened first with `PDFDocumentBuilder::flattenPageTree`
(the same call the remove-links workflow uses), which copies the inherited
MediaBox, CropBox, Resources and Rotate onto the pages.

A rejected request (invalid permutation, page tree that does not match the page
list) leaves the document untouched.

## Limits

- Reordering uses the Editor's existing document-edit policy; encrypted or
  permission-restricted documents are not handled specially.
- Merging PDFs is not part of this change.
- Dragging pages out of the sidebar into another window or application is not
  supported.

## Tests

`UnitTestsViewer`: `pageReorderOrderMath`, `pageReorderInsertionGeometry`,
`thumbnailReorderWorkflow`, `reorderPreservesContentAfterSave`,
`reorderFlattensNestedPageTree`, `viewerThumbnailsAreReadOnly`, and
`nativeThumbnailDragSmoke`.

`nativeThumbnailDragSmoke` drives the real mouse (a worker thread sends `SendInput`
moves and button events) through the Windows drag-and-drop loop, so it moves the
cursor of the machine it runs on and needs the window on top. It runs only with
`FAMILYPDF_NATIVE_DRAG_SMOKE=1` and `QT_QPA_PLATFORM=windows`:

```
$env:FAMILYPDF_NATIVE_DRAG_SMOKE = "1"; $env:QT_QPA_PLATFORM = "windows"
UnitTestsViewer.exe nativeThumbnailDragSmoke
```

The other tests send the drag events (enter, move, drop) to the view directly. Qt
delivers drag events only to a widget that accepts drops, and the view accepts them
only while its own drag runs, so those tests switch `acceptDrops` on for the
duration of the drop.

## Reverse Page Order

The Editor exposes "Reverse Page Order" in Edit and in the thumbnail context
menu, next to Duplicate Pages. Both entries use the thumbnail selection: no
selection or one selected page reverses the whole document; a contiguous range
of two or more pages reverses only that range. For example, selecting pages 2-4
of `1 2 3 4 5 6` gives `1 4 3 2 5 6`.

Non-contiguous selections and documents with fewer than two pages disable the
action. The controller also rejects these requests without changing the document
or adding an Undo step. The Viewer offers no entry and refuses direct requests.
`PDFPageReorder::computeReversedPageOrder` computes the permutation, then the
existing `PDFProgramController::reorderPages` publishes it as one Undo step,
keeping the original document on failure and preserving the page being read.

`UnitTestsViewer`: `reversePageOrderMath`, `reversePageOrderViewerIsReadOnly`,
`reversePageOrderEditorWorkflow`, `reversePageOrderTranslations`, and the Viewer
thumbnail menu check in `insertPagesViewerIsReadOnly`.
