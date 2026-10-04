# Forms and signature workflow v6

Editor fills standard AcroForm text, multiline, checkbox, radio, combo and list
fields through the existing widget editors. Viewer retains its existing reading
role; both applications display signature verification information.

## Values, appearances and history

- Values remain standard `/V`, with choice export values distinct from their
  display labels. List values follow option-index order. ReadOnly is enforced
  by the existing field setters. Password input stays masked and, following the
  existing PDF4QT policy, is not saved to the document.
- LF/CRLF multiline input is normalized for Qt text layout and saved as standard
  line breaks. Field edits retain the document text layer and editable fields.
- Save and close commit the active editor. Save warns about empty required
  fields and allows saving an incomplete draft. Ctrl+S now works because the
  conflicting Notes sidebar shortcut was removed. Save/Undo/Redo shortcuts
  first commit the active form edit.
- Edits use `PDFDocumentModifier`, the existing appearance API,
  `onDocumentModified` and `PDFUndoRedoManager`. No separate history or sidecar.
- If `/NeedAppearances` is true, supported text/choice appearances are refreshed
  through `updateAnnotationAppearanceStreams`. The flag is cleared only when
  every widget has a usable normal appearance and each requested regeneration
  produced a new stream. Failed/unsupported regeneration preserves the flag.
  This fixes Edge discarding generated appearances and showing blank fields.

## Navigation and signatures

Tab/Shift+Tab traverse editable visible widgets, wrap across pages, scroll the
field into view and retain zoom. Page `/Tabs /R` and `/C` use row/column order;
array/default order follows page annotations. Structure-tree (`/S`) ordering
falls back to annotation order. A separate form sidebar was not added.

The existing signature sidebar shows cryptographic validity and certificate
trust separately, along with the field, page, signer certificate and available
reported signing time. Clicking an entry visits its widget. Unknown or disabled
verification remains unknown. Empty unsigned signature fields are not errors.
Original signature appearances are retained instead of a validity overlay.
Editing or Undo/Redo clears verification results for the old byte revision.

The OpenSSL verifier and certificate stores are unchanged. Signature coverage
warnings describe uncovered bytes; they do not classify the nature or legitimacy
of later edits. The existing full-rewrite save is unchanged and does not preserve
cryptographic signatures in edited signed PDFs. Reopen to verify saved bytes.
No signing workflow, certificate installation or JavaScript validator was added.

## Validation

- Windows Release: six v6 integration cases passed (8 with setup/cleanup),
  covering values, ReadOnly/Required, multiline, max length, password persistence
  policy, actual Ctrl+S, Tab/Shift+Tab, cross-page focus, Undo/Redo, Save, Save As,
  reopen, standard AP streams, unchanged page Contents and malformed fields.
- Public MIT-licensed pyHanko fixture: two cryptographically valid, untrusted
  signatures; tampered bytes fail verification; malformed signature values do
  not crash. UI tests exercise valid/untrusted, invalid and unknown states.
- Appearance fallback test retains NeedAppearances when an invalid rectangle
  prevents appearance regeneration. The normal workflow clears it after all
  appearances are available, and Undo/Redo retains matching appearance objects.
- Related forms tests: 4 passed; document edit tests: 23 passed. Focused reading,
  search, thumbnails, page management, reading position and annotation smoke
  passed. Full-suite validation is delegated to CI.
- Native dist smoke uses clean PATH and checks that Qt/plugins are loaded from
  the package. Editor typing, Save, Undo/Redo and navigation were exercised;
  Viewer reopened the saved form and displayed valid/untrusted signatures.
- Installed Edge displayed saved text, multiline, checkbox/radio and second-page
  list selection. UI Automation read the saved text and multiline values from
  Edge's actual form controls. Evidence is local under `build/forms-v6` (ignored).

`dist/FamilyPDF` contains the updated Release binaries and Traditional Chinese
translations; the existing Qt/FFmpeg runtime and plugins are retained.
