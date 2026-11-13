# Quiet document workspace

## Before / after

| Surface | Before | Proposed |
| --- | --- | --- |
| Empty Reader / Editor | Blank canvas with document controls | FamilyPDF and one Open PDF button |
| Single document | Repeated document tab | Tab hidden until multiple documents exist |
| Navigation | First / previous / next / last | Previous / page number / next; remaining actions in menus |
| Zoom and selection | Several parallel modes | Zoom value, fit width, text selection; other modes in menus |
| Editor | Save As, optimization, annotation variants | Save and highlight visible; secondary tools remain in menus |
| Plugin toolbars | Every loaded plugin contributes a toolbar | Hidden by default, available from toolbar context menu; saved layouts preserved |

The design reuses existing QActions and controller flows. It does not remove PDF capabilities, change file formats, upload documents, or create an account system. The empty-state button follows the existing Open action's enabled state and has a localized accessible name.

## Validation

Added Qt tests cover the Open action connection and enabled state, empty/document transitions, and Reader/Editor open-close-reopen lifecycles. They also assert that secondary actions still exist outside the primary toolbar.

Static validation: XML parsing for edited UI/translations, CRLF preservation, and git whitespace checks. No project build or Qt test execution was performed while preparing this change, in accordance with AGENTS.md. Automatic Windows PR CI is the runtime gate, not a substitute for manual UX testing.

## Manual release checks still required

- Empty launch: keyboard reaches Open PDF; cancel chooser returns to the same home.
- Open regular PDF; navigate pages; use menus for removed toolbar actions.
- Password PDF: cancel, wrong password, then correct password; no document data is uploaded.
- Editor Save / Save As: save only a disposable test copy; cancel returns without losing work.
- Close with unsaved changes: Cancel keeps the document and edits; discard returns home.
- Close and reopen a document; multiple-document tabs remain discoverable.
- Verify persisted toolbar choices, focus, screen-reader labels, and Traditional Chinese text.

There is no account login or web submission flow in this Qt desktop application. Password-protected PDF opening, local saving, and cancel/close navigation are the corresponding flows. These manual checks have not yet been executed in a Windows GUI.
