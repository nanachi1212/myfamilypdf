# Search experience v4

The shared Viewer/Editor Find tool now searches while typing and publishes page-ordered results before the entire document finishes. Ctrl+F focuses/selects the query, Enter advances, Shift+Enter goes back, and Esc closes Find. Clearing the query clears highlights. The status shows the current match and count, a trailing `+` while searching, no results, or an incomplete-search warning.

## Execution and lifetime

The existing PDFTextLayoutGenerator/PDFTextFlow perform extraction and matching. One QtConcurrent worker per Find tool owns a document snapshot and worker-local font/optional-content state. A 120 ms typing delay coalesces changes; an atomic cancellation flag plus a generation check prevents obsolete results from reaching the UI. Replacement work starts after the previous future finishes. Document reset/content edits, document-tab switches, closing Find and destruction cancel the worker; destruction joins it. No persistent index or dependency is introduced.

Results remain in page/text order. Next at the currently known frontier waits for the next result; wrap-around waits until completion. Only visible-page highlight geometry is assembled. Navigation preserves zoom/layout and centers the current match. A no-text/OCR hint requires a successful complete scan with no text and no optional-content groups; extraction errors are reported as incomplete instead.

## Windows Release measurements

Same generated embedded-font Chinese/English PDFs on the same Windows 11 machine. Cold Find, query `alpha`, native Qt elapsed timer and 10 ms UI heartbeat. Before: main `989b501a4000e1d9870b2635b7fbe35bb93b326a`. After includes typing debounce. These are local samples, not performance guarantees.

| Fixture | Matches | Before first / complete | After first / complete | Maximum UI heartbeat gap before / after |
| --- | ---: | ---: | ---: | ---: |
| 12 pages | 48 | 93 / 93 ms | 149 / 172 ms | 28 / 14 ms |
| 1,200 pages | 4,800 | 4,145 / 4,145 ms | 127 / 2,322 ms | 118 / 31 ms |
| 3 raster-only pages | 0 | 42 ms, ambiguous empty Find | 151 ms, explicit no-text/OCR hint | after 15 ms |

Small-document latency includes the intentional debounce; the main gain is early large-document results and responsive query changes. The old implementation also required committing the query and waited for whole-document text layout before returning matches.

## Validation

`UnitTestsViewer searchExperience searchCancellationAndDocumentLifecycle` uses self-contained fixtures, including a Type 3 ToUnicode font with deterministic Chinese mappings. It covers both applications, Chinese/mixed/English queries, case sensitivity, keyboard/button navigation, cross-page/same-page matches, no results, empty/whitespace query, Esc, progressive results, rapid changes, scrolling during search, tab switching, document close/reopen and app destruction. `searchPerformanceBenchmark` optionally reads external fixtures from `FAMILYPDF_SEARCH_FIXTURES`; fixture files and local logs stay outside Git.

Targeted extraction, thumbnail/page-management, reading-position and context-menu regressions and font-encoding tests also pass locally. Release is rebuilt with the existing build script, deployed to `dist/FamilyPDF`, and exercised with a PATH containing only Windows/System32 and Windows. Qt/FFmpeg/plugin runtime files remain self-contained. No signing, system security or package-policy changes are required.
