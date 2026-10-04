# Print and image export workflow v7

Viewer and Editor share one print dialog and one image export dialog. Both
reuse the existing page pipeline (`PDFRenderer`, `PDFRasterizerPool`,
`PDFAnnotationManager`); no renderer code was changed.

## Entry points

| Where | What |
|---|---|
| File > Print (Ctrl+P) | Print dialog with preview |
| File > Export Page(s) as Images... | Image export dialog (current page, all pages, selected thumbnails, page range) |
| Sidebar thumbnails, context menu | Print Selected Pages..., Export Selected Pages as Images... |
| Page context menu (text selected) | Export Selection as Image... |

The Editor keeps its existing advanced "Render to Images..." dialog.
Traditional and Simplified Chinese translations are included.

## Print

`PDFPageOutput::print` (`Pdf4QtLibGui/pdfpageoutput.*`) paints the requested
pages one by one onto a `QPrinter` with the vector print pipeline: text and
vector graphics stay vectors, images are passed as images. No page is rendered
before it is printed, and nothing is rasterized in advance. Annotations are
drawn with `PDFAnnotationManager` (print target), so annotations without the
Print flag are not printed.

- Pages: all, current page(s), pages selected in the thumbnails, or a range such
  as `1-3,8,10-12`. Pages are printed in document order, each page once.
  Out-of-range input is rejected with a message.
- Orientation: automatic (every sheet follows the orientation of its page, set
  per sheet through `QPrinter::setPageOrientation` before `newPage()`),
  portrait or landscape.
- Scaling: fit to the printable area (scales up and down, keeps the aspect
  ratio) or actual size (100%, centered on the sheet like the physical page).
  Content outside the printable area is clipped.
- Printer, paper size, color/grayscale, copies and collate come from the
  system printer list. Duplex is passed to the printer as chosen and is only
  offered when `QPrinterInfo::supportedDuplexModes()` reports it. It is never
  emulated.
- Copies: when the print engine has no native copies (for example Qt's PDF
  writer) the copies are produced by repeating the pages here, collated or
  not as requested. Windows drivers receive the copy count.
- Cancel: the Cancel button of the progress dialog stops before the next
  sheet, aborts the job and removes an explicitly named output file.
  Cancelling the print dialog leaves nothing running.
- The page area is the **MediaBox** (rotated by `/Rotate`) exactly as the
  viewer shows it; the content is clipped to the CropBox when the viewer
  option for it is on (default). The page keeps its media box size, it does
  not shrink to the crop box.
- View-only modes (invert, grayscale, high contrast, bitonal, custom colors,
  debug overlays, render times) are not part of printed or exported output.
- Windows note: `Microsoft Print to PDF` asks for a file name. Qt writes the
  PDF itself, without the driver, when an output file name ends with `.pdf`.

### Preview

The print dialog contains a one-sheet preview with sheet navigation. It draws
the sheet (paper size, margins, orientation) and renders only the page being
looked at, in a background thread, so opening the dialog does not depend on the
size of the document. `QPrintPreviewDialog` was not used: it renders every page
of the job when it opens.

## Image export

`PDFPageImageExporter` renders pages with the existing rasterizer
(`PDFRasterizerPool`, Blend2D) and writes PNG or JPEG files.

- Defaults: PNG, 150 DPI, JPEG quality 90. Resolution 36 to 1200 DPI. An image
  may have at most 120 million pixels; larger requests are refused before any
  memory is allocated.
- The page is flattened onto white paper (the rasterizer leaves unpainted
  areas transparent, which JPEG would turn black). The resolution is stored in
  the file.
- One file per page. Names are `<document>_p<number>.<png|jpg>`, with the
  number padded to the width of the page count so files sort in page order, and
  `<document>_p<number>_selection.<ext>` for selections. Existing files are never
  replaced without asking.
- Files are written through `QSaveFile`: a failed write never leaves a partial
  file and never damages an existing one.
- Only the requested pages are compiled and rendered. Pages run in parallel with
  as many rasterizers as the memory budget allows.
- The result lists every file. A run in which a page failed or was cancelled is
  reported as such ("N of M saved"), never as complete. Cancel stops before the
  next page; finished files are kept and counted.
- Export of a selection uses the bounding box of the selected text on each
  page (the geometry the text markup tools use), plus 2 points of margin.

## Verification

`UnitTestsPrintExport` (new) renders printed and exported pages with the product
renderer and compares what a user would see: page order and identity, sheet
orientation, fit and actual size placement, copies and collation, cancel, crop
box, rotation (90/180/270), mixed page sizes, transparency, annotations, form
field appearance, Chinese text, a JPEG2000 scan, regions, partial failure,
cancel, naming and limits. `UnitTestsViewer` gained UI checks for the menu
entries, the thumbnail menu, both dialogs, the selection context menu and a
filled form. The Windows driver path (`Microsoft Print to PDF`) is exercised
when the `windows` platform plugin and the driver are available.

Fixture: `UnitTests/fixtures/print-export-jpx.pdf` is generated for this
project (synthetic 300 x 400 scan, JPEG2000 stream, no third-party content).

## Measured performance (Release, local, 4 rasterizers)

| Case | Time |
|---|---|
| Print dialog until the first preview page (1,200 page document) | about 280 ms (about 150 ms for a 3 page document) |
| 1 page PNG, page 600 of 1,200, 150 DPI, text page | 29 ms |
| 10 pages PNG, pages 601-610 of 1,200, 150 DPI | 84 ms |
| Print page 600 of 1,200 to PDF output | 14 ms |
| Print page 251 of 300 / export pages 321-322 of 400 (synthetic) | 73 ms / 22 ms |
| A4 PNG at 600 DPI (4958 x 7017) | about 680 ms |
| JPEG2000 scan page, 300 x 400 | 15-45 ms |

Only the requested pages are compiled, so the time does not grow with the
page count; no optimization was needed.

## Limits

- Printing is sequential on the GUI thread (a QPainter on a printer cannot be
  shared); cancel is checked between sheets.
- The preview is a raster approximation of one sheet; the printed output is
  vector.
- Region export is available for text selections only; there is no marquee
  selection export in the page view (the existing Editor screenshot tool still
  copies a region to the clipboard).
- Image export honors the viewer's annotation display option; a page rendered
  at very high resolution needs memory for the whole page before it is cropped
  (regions).
