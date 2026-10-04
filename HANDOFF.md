# FamilyPDF AI 交接文件

更新日期：2026-10-04（Asia/Taipei）。本文件是 Claude、Codex 與其他 AI 的共同交接入口。

## 接手與更新規則

- 開始工作先讀 `AGENTS.md` 與本文件，再確認 `git status --short --branch`；只讀取當次任務相關文件與程式碼。
- **每完成一階段任務，必須先更新本文件，再回報完成或進入下一階段。** 階段指可交接的成果，例如實作、驗證、review 修正、交付，不是每次命令或輪詢。
- 暫停、遇到 blocker 或交接給其他 AI 時，也要更新：目前目標、已完成／未完成、修改檔案、驗證證據、Git／PR 狀態與下一個具體動作。
- 更新現有段落並保留必要決策；不要堆疊完整對話、command log 或過期 SOP。未執行的測試必須標示未驗證，等待中的 CI 不算通過。
- 本文件的狀態是更新當時的快照；接手時只重查與新任務有關、可能變動的狀態。不得寫入 secrets、私鑰、密碼或使用者文件內容。

## 目前狀態與下一步

- 專案：`F:\Projects\Codex project\myfamilypdf`，Windows x64 PDF 閱讀／編輯工具。
- 「列印與圖片匯出工作流 v7」已完成並 merge（PR #15，squash，merge commit `60be9699b42d01a42b7860e153ccd77868b191b1`）；CodeQL、FamilyPDF validation、CI 全部 PASS 後才合併。沒有進行中的功能任務或已知 blocker；下一步依使用者新需求開始，不必重做 v6／v7。
- `.ai-memory.toml` 是使用者原有 untracked 檔案，保留，不修改、不提交。
- 先前整理的 `HANDOFF.md`／`AGENTS.md`／`CLAUDE.md`（v6 後的交接文件重寫）原本未提交，已隨 v7 PR 一起提交。

## v7 已交付內容

- Viewer／Editor 共用：File → Print（含預覽）、File → Export Page(s) as Images…、縮圖右鍵「列印選取的頁面／匯出選取的頁面為圖片」、文字選取後右鍵「Export Selection as Image…」。Editor 原有進階「Render to Images…」不變。
- 列印：`PDFPageOutput::print`（`Pdf4QtLibGui/pdfpageoutput.*`）沿用向量列印管線逐頁輸出，不預先 rasterize、不預先處理未列印頁；註解以 `PDFAnnotationManager`（Print target）繪製。支援全部／目前頁／縮圖選取／範圍（`1-3,8,10-12`，依文件順序、不重複）、自動／直向／橫向（逐張依頁面方向切換）、符合可列印區域／實際大小、印表機、紙張、色彩、份數／逐份、雙面（只在 `QPrinterInfo` 回報支援時提供，不模擬）。無原生份數的引擎（PDF 輸出）由程式重複頁面。取消在下一張前生效並中止工作、移除指定輸出檔。
- 預覽：列印對話框內的單張預覽（`pdfprintdialog.*`），背景只渲染正在看的那一頁；未用 `QPrintPreviewDialog`，因為它開啟時會渲染整份工作的所有頁面。
- 圖片匯出：`PDFPageImageExporter` 沿用 `PDFRasterizerPool`／`PDFRenderer::compile`／`PDFAnnotationManager`，輸出 PNG／JPEG（預設 PNG、150 DPI、JPEG 品質 90；36–1200 DPI；單張上限 1.2 億像素）。白底平面化（rasterizer 對未繪製區域為透明，JPEG 會變黑）、檔名 `<文件>_p<頁碼補零>.<ext>`（選取為 `_selection`）、`QSaveFile` 原子寫入、覆蓋前詢問、取消／部分失敗如實回報「N / M 已儲存」。選取範圍匯出使用文字選取的邊界框（與標記工具同一幾何）。
- 頁面區域沿用畫面規則：以旋轉後 MediaBox 為輸出範圍，內容依 ClipToCropBox 裁切，CropBox 小於 MediaBox 時尺寸仍為 MediaBox。列印／匯出不帶畫面專用模式（反相、灰階、高對比、除錯疊圖、渲染時間）。
- 繁中／簡中翻譯已補（`translations/PDF4QT_zh_TW.ts`、`PDF4QT_zh_CN.ts`）。說明文件：`docs/print-export-v7.md`。
- 未修改 renderer／色彩管理／搜尋／OCR／表單與簽章核心；未新增或升級 dependency。

## 驗證與交付證據

- 新測試 `UnitTestsPrintExport`（35 項，offscreen 33 通過＋2 項因無 Windows 字型／平台外掛而 SKIP；`QT_QPA_PLATFORM=windows` 下 35/35，含 Microsoft Print to PDF 驅動與中文字型）與 `UnitTestsViewer` 新增 6 項（入口／縮圖選單、列印對話框、匯出對話框、選取匯出、填寫表單列印與匯出、1,200 頁量測）。完整 CTest 10/10 通過。
- 效能（本機 Release）：1,200 頁文件開列印對話框至預覽第一頁約 280 ms；第 600 頁 1 頁 PNG 29 ms；10 頁 PNG 84 ms；列印第 600 頁 14 ms；A4 PNG 600 DPI 約 680 ms。只輸出少數頁時時間不隨頁數增加，不需優化。
- `dist/FamilyPDF` 只更新自家二進位（exe、`Pdf4QtLib*.dll`、`pdfplugins`、zh qm）；第三方 DLL 未動。Viewer／Editor clean-PATH smoke 通過。

## Git 狀態

- PR #15 已 squash merge 為 `60be9699`，本機 `main` 已同步，feature branch 已刪除（本機與遠端）。本機 `gh` 仍未登入，無法自行建立 PR 或查 CI；merge 由使用者在 GitHub 完成。

## 限制與接手注意事項

- 列印在 GUI 執行緒逐頁進行（`QPainter` 不能跨執行緒），取消只在張與張之間檢查；預覽是單張光柵近似，實際輸出為向量。
- `Microsoft Print to PDF` 會跳出存檔對話框；Qt 對 `.pdf` 輸出檔名會自己寫 PDF 而不走驅動，驅動路徑測試用非 `.pdf` 副檔名。
- 區域匯出只支援文字選取；高 DPI 區域匯出會先渲染整頁再裁切（受 1.2 億像素上限保護）。
- 原有限制仍適用：密碼欄位不保存、`/Tabs /S` 以 annotation order 代替、現有 full-rewrite save 不保留已簽署 PDF 的原簽章有效性（見 `docs/forms-signatures-v6.md`）。
- **本機建置環境（重要）**：系統為 zh-TW，MSVC 的 `/showIncludes` 前綴為中文，CMake 偵測成亂碼，導致 Ninja 不追蹤標頭（`ninja -t deps` 為 `#deps 0`）。本輪已把 `build/phase0-upstream-release/CMakeFiles/rules.ninja` 的 `msvc_deps_prefix` 改為 `注意: 包含檔案:` 並完整重建；CMake 重新產生時會被還原，需重做。另外重新 configure 曾觸發 vcpkg 移除並重建失敗（`vcpkg_installed` 被清空），已由 `FamilyPDF-tools/binary-cache` 的 zip 還原，並以 `-DVCPKG_MANIFEST_INSTALL=OFF` 固定；build 目錄內第三方 DLL 因此與 `dist` 的 hash 不同（同版本）。細節見使用者記憶 `familypdf-local-build-quirks`。
- 工具位於同層 `FamilyPDF-tools`；既有 build 為 `build/phase0-upstream-release`。本機不要為文件更新或 CI 等待重跑 Full。

## 需要深入時再讀

- v7 行為與限制：`docs/print-export-v7.md`；核心 `Pdf4QtLibGui/pdfpageoutput.*`，對話框 `pdfprintdialog.*`、`pdfexportimagesdialog.*`，流程 `pdfprogramcontroller.cpp` 的 `runPrintWorkflow`／`runExportImagesWorkflow`／`exportSelectionAsImage`。
- v7 回歸：`UnitTests/tst_printexporttest.cpp`；`tst_viewercontextmenutest.cpp` 的 `printAndExportEntriesAreAvailable`、`printDialogOptionsAndCancel`、`exportImagesDialogWorkflow`、`exportSelectionAsImageWorkflow`、`filledFormPrintsAndExports`、`printExportLargeDocumentBenchmark`。
- v6 行為與限制：`docs/forms-signatures-v6.md`（表單核心 `pdfform.*`、儲存／UndoRedo `pdfprogramcontroller.cpp`）。
- 其他工作區背景：`docs/WORKSPACE-HANDOFF.md`，視新任務需要讀取；舊快照不能取代本文件或現場狀態。