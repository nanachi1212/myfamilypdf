# Search v4.1: warm document text

## Root cause and choice

PR #11 performs `processContents`, `createTextLayout`, and `createTextFlows` again for every query. A same-fixture instrumented Windows Release baseline measured extraction at 749-806 ms, layout construction at 1,487-1,506 ms, text-flow construction at 46-51 ms, matching at 2-9 ms and page sorting below 1 ms. Across the five measured queries, GUI result delivery totalled 69 ms and highlight painting 9 ms. Neither UI delivery nor highlight explains the repeated 2.5-second scan.

The pre-v4 105 ms Chinese search used the completed `PDFTextLayoutStorage`: compressed layouts were decompressed and searched in parallel, without PDF extraction or layout construction. Reusing that compressed representation incrementally cost about 4,831 ms cold / 1,573 ms warm locally. Keeping full immutable layouts avoided that CPU cost but added about 880 MiB for this fixture. Neither candidate is shipped.

The shipped implementation retains the existing `PDFTextFlow` output (text, character pointers and bounding rectangles) once per successfully completed page, under the existing text-layout compiler's document session. If the legacy full layout cache is already ready, Find takes an owned, implicitly shared snapshot and derives flows from it. Otherwise the existing v4 extraction path produces flows progressively. There is no disk index, database, new matching algorithm, renderer/OCR change or dependency change.

## Ownership and invalidation

The compiler owns one shared search-cache object. At most one Find worker accesses its page vectors at a time; replacement queries start after the previous future finishes. Completed page entries are never modified, including successful empty pages. Query cancellation, clear and Esc retain safely completed entries. Failed/interrupted extraction is not cached.

`stop(true)` replaces the cache owner through the existing reset/page-content/optional-content/features invalidation path and cancels/restarts an active query. An old worker owns its document snapshot and retired cache, so it cannot populate a new document's cache. The existing query generation gate rejects stale batches. Destruction cancels and joins the Find worker. Weak-owner tests verify that document close actually releases the cache, and reopen gets a different empty owner.

The legacy layout future is consumed before invalidation and then released. Its later queued completion cannot reinstall an old document's layout. Editor content replacement and Undo exercise this path. The legacy layout result now carries full-extraction success provenance. Find reuses that storage only when every page succeeded; otherwise it uses normal extraction and caches only successful pages. A malformed page containing valid text followed by an invalid graphics-state restore verifies that partial legacy text never becomes a successful warm-cache entry.

## Windows Release evidence

Same `build/search-v4/text-1200.pdf` fixture from PR #11: 1,200 pages, embedded Chinese/English fonts, 4,800 alpha matches, 2,400 Chinese matches and one end-page match. Native timers include the 120 ms debounce. UI Automation timings additionally include desktop automation overhead. These are measured local samples, not performance guarantees.

| Native measurement | v4 baseline this run | v4.1 |
| --- | ---: | ---: |
| Cold first result | 122 ms | 131 ms |
| Cold complete | 2,469 ms | 2,513 ms |
| Warm same query | 2,561 ms | 131 ms |
| Warm Chinese query | 2,535 ms | 142 ms |
| Warm last-page query | 2,523 ms | 127 ms |
| Maximum heartbeat gap, ordinary queries | 14 ms | 14 ms |
| A → AB → ABC (includes two 150 ms typing intervals) | 2,848 ms | 431 ms |

PR #11's earlier historical sample was 127 ms first / 2,322 ms complete, with repeated UI queries around 2.6-2.7 seconds. Instrumented v4.1 warm queries extracted **zero pages**, built **zero layouts/flows**, and spent 2-6 ms matching plus less than 0.1 ms sorting. Temporary profiling instrumentation is excluded from the product diff.

| Clean-PATH desktop completion | Viewer v4 → v4.1 | Editor v4 → v4.1 |
| --- | ---: | ---: |
| Cold alpha | 2,416 → 2,398 ms | 2,593 → 2,523 ms |
| Same alpha after clear | 2,446 → 196 ms | 2,461 → 218 ms |
| Chinese query | 2,425 → 185 ms | 2,454 → 217 ms |
| Last-page query | 2,401 → 190 ms | 2,400 → 192 ms |
| Final ABC after rapid replacement | 2,392 → 212 ms | 2,396 → 184 ms |

| v4.1 process memory (working set / private, MiB) | Viewer | Editor |
| --- | ---: | ---: |
| Document open | 160.30 / 115.90 | 155.17 / 110.45 |
| Cold search complete | 305.43 / 262.00 | 299.53 / 256.39 |
| Warm same query | 305.75 / 262.38 | 300.18 / 256.76 |
| Clear after query/navigation sequence | 324.08 / 281.48 | 296.54 / 252.98 |
| Document closed | 146.38 / 101.59 | 164.02 / 121.94 |

Cold-search working-set growth is 145.13 MiB Viewer / 144.36 MiB Editor, versus 13.40 / 14.89 MiB in v4 (an incremental cost of about 132 / 129 MiB). Warm searches do not add another document-sized copy. Clear intentionally retains text; close releases it. Baseline post-close private memory was 101.68 MiB Viewer / 122.88 MiB Editor, comparable to v4.1's 101.59 / 121.94 MiB.

## Validation

- `UnitTestsViewer searchExperience searchCancellationAndDocumentLifecycle searchTextCacheOwnership searchCacheReuse searchEditorContentInvalidation searchWarmCacheBenchmark`: 11 passed. Both Viewer and Editor cover progressive results, navigation, Unicode, rapid replacement, clear/Esc, no results, no-text hint, document switch, close during search and destruction during search.
- New cache tests cover successful empty pages, shared ownership, cancelled partial-page reuse, completed warm reuse, legacy full-layout reuse, document-close release, content replacement, stale legacy completion and Undo invalidation.
- Release build completed using the existing build script. The deployed `dist/FamilyPDF` has 80 x64 PE binaries with no missing direct/delay imports, including Qt/FFmpeg/plugins. Desktop smoke uses only Windows/System32 and Windows on PATH and rejects developer-toolchain DLL loads.
- Local artifacts, fixtures, timing logs and screenshots remain under ignored `build/search-v4.1`. `.ai-memory.toml` is untouched. Full tests are delegated to the existing CI jobs.

- One Codex GitHub review identified partial legacy-layout success provenance (P2). The consolidated fix adds verified extraction status and `searchRejectsPartialLegacyLayout`; the follow-up targeted suite passed 11 tests, including cancellation/lifetime and Editor invalidation. No repeat review loop.
