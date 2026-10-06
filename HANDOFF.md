# FamilyPDF AI 交接文件

更新日期：2026-10-06（Asia/Taipei）。本文件是 Claude、Codex 與其他 AI 的共同交接入口。

## 接手與更新規則

- 開始工作先讀 `AGENTS.md` 與本文件，再確認 `git status --short --branch`；只讀取當次任務相關文件與程式碼。
- **每完成一階段任務，必須先更新本文件，再回報完成或進入下一階段。** 階段指可交接的成果，例如實作、驗證、review 修正、交付，不是每次命令或輪詢。
- 暫停、遇到 blocker 或交接給其他 AI 時，也要更新：目前目標、已完成／未完成、修改檔案、驗證證據、Git／PR 狀態與下一個具體動作。
- 更新現有段落並保留必要決策；不要堆疊完整對話、command log 或過期 SOP。未執行的測試必須標示未驗證，等待中的 CI 不算通過。
- 本文件的狀態是更新當時的快照；接手時只重查與新任務有關、可能變動的狀態。不得寫入 secrets、私鑰、密碼或使用者文件內容。

## 目前狀態與下一步

- `UnitTestsViewer` 的間歇性 Save As 卡死已用 full dump 與 live non-invasive attach 定位為 test harness lifecycle，不是 search／writer／文件 reset：主執行緒停在 `QFileDialog::accept -> QFileDialogPrivate::itemAlreadyExists -> QMessageBox::warning -> QDialog::exec`；timer 正同步執行 `accept()`，不能重入處理內層 overwrite modal。其他 PDF compiler、file gatherer、watcher 與 thread-pool threads 都在正常 wait。`thumbnailSelectionAndPageManagement` 現為每次 invocation 配置唯一 extract／Save As 輸出，並直接設定 nonnative `fileNameEdit`、在 `accept()` 前停止 timer。Release offscreen 最小序列同 process 20/20、targeted 8/8、完整原順序 3/3（每次 65 passed／4 conditional skips）PASS；尚待 commit／push／PR／CI。
- 專案：`F:\Projects\Codex project\myfamilypdf`，Windows x64 PDF 閱讀／編輯工具。
- 「縮圖拖曳重排頁面 v8」已 squash merge（PR #17，main `7cfa289b1f7e2a5c88c695062344c71870896c3d`，Windows／Ubuntu／runtime／CodeQL 全綠），不必重做。
- 「Merge PDFs v9」已在 branch `feature/merge-pdfs-v9`（自 `7cfa289b` 建出）實作並通過本機驗證；commit／push／PR／CI／squash merge 的狀態見下方「Git 狀態」。本機 `gh` 未登入，由 ChatGPT／使用者建立 PR。
- 先前「列印與圖片匯出 v7」已 merge（PR #15，squash，merge commit `60be9699b42d01a42b7860e153ccd77868b191b1`），不必重做 v6／v7。
- `.ai-memory.toml` 是使用者原有 untracked 檔案，保留，不修改、不提交。

## v9 已交付內容（Merge PDFs）

- File → Merge PDFs...（Viewer／Editor 皆有，無開啟文件也可用）：選多份 PDF → 排序／選頁 → 合併 → 另存為「新的 PDF」。不修改來源或目前文件、不進 Undo、不自動關閉目前文件；成功後可選「Open Merged PDF」（有開啟文件時用既有 `openDocumentInNewTabRequested` 開新分頁）。
- 對話框 `Pdf4QtLibGui/pdfmergepdfsdialog.*`：Add Files、Add Open Document（記憶體中的目前文件，含未存修改）、Remove、Move Up/Down、列表拖曳排序、每列檔名／頁數／頁面範圍（可編輯）、輸出檔＋Browse；覆蓋前詢問；輸出不得等於清單中的來源。
- 輸出順序＝清單順序 → 各列頁面範圍順序（`3,1` 先第 3 頁再第 1 頁、可重複、`all`／「所有頁面」／空白＝全部）。既有 `PDFClosedIntervalSet::parsePageSelection` 會排序去重（列印／提取用），所以 v9 用 `PDFDocumentMerger::parsePageList`。
- 引擎 `Pdf4QtLibCore/sources/pdfdocumentmerger.*`：重用 `PDFDocumentManipulator::assemble()` 多文件路徑（不建立第二套 object importer）、`PDFDocumentReader`＋密碼 callback（最多 3 次）、`PDFSecurityHandler::isAllowed`、`QSaveFile` 原子寫入（失敗／取消不動目的檔）。Manipulator/Writer 沒有 progress／cancel，所以用 `QtConcurrent` worker＋忙碌條＋Cancel，只在階段之間檢查取消。
- 對 manipulator 的三個小改動（皆 opt-in 或外觀，PageMaster 行為不變）：`setDocumentCaption()`（大綱用檔名而非 "Document 0"）、`setAttachMergedCatalogObjects()`（`finalizeMergedObjects()` 上游從未被呼叫，多文件合併會丟掉 AcroForm／OCProperties；開啟後表單欄位、值、optional content 保留；**不**掛 Names，因 `mergeNames` 把 key 寫成 name 而非 string，且會把未選頁面拖進檔案）。另有 `pruneExcludedPages()`（合併後只在需要時執行）：頁面複製會把「被保留頁面連到的、或帶有表單欄位的」未選頁面當未使用物件留在檔案內，此步驟移除只在未選頁面上的欄位、把殘留參照改 null、刪除未使用物件。
- PR #18 correctness review 修正（2026-10-06）：`collectObjectsAndCopyPages` 合併 AcroForm／OCProperties 前把指向 array／dictionary 的項目轉成直接物件（`dereferenceForMerge`）——原本 `/Fields`、`/DR`、`/OCGs` 是間接參照時會丟掉前面來源的欄位，或（先直接、後間接）丟出 `std::bad_variant_access` 穿出 worker；PageMaster 同受惠。`finishMergedForm()` 移除 `/XFA`、任一來源 `NeedAppearances true` 就保留 true。`mergeToFile` 把例外轉成錯誤訊息。重複頁的連結失效／欄位不在 `/Fields`、表單層級預設值以最後來源為準，已記入 `docs/merge-pdfs-v9.md` A 類限制。
- 上游限制分類（細節見 `docs/merge-pdfs-v9.md`）：A 接受並文件化＝tagged structure tree／文件動作／threads 被移除、連到未選頁面的連結失效、部分頁面來源的書籤不保留；B 合併前警告＝加密來源（輸出不加密）、數位簽章（輸出必為無效，已用 OpenSSL 驗證）、不同來源同名表單欄位（不改名，可能共用值）、XFA、具名目的地；C 阻止＝無法開啟／密碼錯誤、來源權限未同時允許 copy 與 assemble、未選頁面、輸出等於來源。
- 繁中／簡中已補（`PDF4QT_zh_TW.ts`、`PDF4QT_zh_CN.ts`：menu、`pdfviewer::PDFMergePdfsDialog`、`pdf::PDFDocumentMerger`、`Untitled`）。文件：`docs/merge-pdfs-v9.md`。沒有新增或升級 dependency，沒有改 writer／加密／簽章核心／AcroForm merge 核心。

## v9 驗證證據（本機 Release）

- `UnitTestsMergePdfs`（新，Core 引擎、真實檔案、寫出後讀回）：45 通過＋1 skip（`writeSmokeArtifacts` 需環境變數；含新 `formAndLayersWithIndirectEntries` 與頁 `3,1,3` 三個獨立頁面物件）。涵蓋解析器（順序／重複／錯誤）、2／3 份來源、範圍／自訂順序／重複頁、混尺寸＋旋轉＋CropBox、註解與 /P、AcroForm 同名／不同名、未選頁面與欄位與連結不殘留、書籤、具名目的地警告、跨來源連結、optional content、JPEG 2000 位元組相同、加密來源（密碼／錯誤密碼／取消／權限不足）、簽章來源（警告＋輸出無有效簽章）、覆蓋、來源＝目的、取消與失敗原子性、來源不變。
- `UnitTestsViewer` 新增 6 項：`mergePdfsEntriesAreAvailable`、`mergePdfsDialogWorkflow`、`mergePdfsOutputOrderAndTextLayerAfterReopen`（輸出在 FamilyPDF 重開、每頁文字層）、`mergePdfsBlocksAndWarns`、`mergePdfsCancelLeavesNoPartialFile`、`mergePdfsTranslations`：全 PASS。
- 回歸 smoke PASS：v8 重排（`pageReorder*`、`thumbnailReorderWorkflow`、`reorderPreservesContentAfterSave`、`reorderFlattensNestedPageTree`、`viewerThumbnailsAreReadOnly`）、提取、列印／匯出（`printAndExportEntriesAreAvailable`、`printDialogOptionsAndCancel`、`exportImagesDialogWorkflow`、`exportSelectionAsImageWorkflow`、`filledFormPrintsAndExports`）、`menuActionsOperateOnTheDocument`、繁中資源；`UnitTestsDocumentEdit` 23、`UnitTestsForms` 4、`UnitTestsBookmarks` 20、`UnitTestsSecurity` 6。
- 既有 flaky（非 v9 regression，已用 main 基準確認）：`UnitTestsViewer` 依序跑 `searchExperience`→`thumbnailSelectionAndPageManagement`（offscreen、300 秒 watchdog），main@`7cfa289b`（`build/baseline-main-build`，worktree `build/baseline-main-src`）3 次卡 2 次、PR 3 次卡 3 次，卡點相同：`deletePages({5})` 後的 `performSaveAs()`（之後的 `QFile::exists` 未執行），main 沒有任何 merge 程式碼。根因未查（需另開任務）；不可用 sleep／放寬 timeout 掩蓋。
- 效能（Release、合成小檔，只記錄）：2 份小檔 4 ms；10 份×20 頁（200 頁）約 20 ms；5 份×200 頁（1,000 頁）約 78 ms；JPEG 2000 掃描頁×500（500 頁）載入 186 ms、合併寫出 102 ms。無瓶頸。
- `dist/FamilyPDF` 只更新自家二進位（exe、`Pdf4QtLib*.dll`、`pdfplugins`、zh qm）；Viewer／Editor clean-PATH 啟動並開啟合併檔 PASS（載入的模組皆來自封裝資料夾，另有輸入法注入的 DLL）。
- NOT_TESTED：Edge 開啟合併結果的目視檢查（範例檔可用 `FAMILYPDF_MERGE_ARTIFACT_DIR=<dir> UnitTestsMergePdfs` 產生，`build/merge-v9` 已有一份）；列表以滑鼠拖曳排序；XFA 警告。

## v8 已交付內容（縮圖拖曳重排）

- Editor 左側縮圖可拖曳一頁或 Ctrl／Shift 多選後一起拖曳重排；Viewer 縮圖維持唯讀（不能拖、不接受 drop）。被拖頁面保持彼此相對順序；drop 在自己原區域不產生變更也不建立 Undo。重排後被移動的頁面維持選取、正在看的頁面維持不變，側邊欄停留在縮圖頁（連 Undo／Redo 後也是）。
- 流程：`PDFThumbnailsListView` 算出 drop 位置 → `PDFSidebarWidget` 算 `newPageOrder`（`PDFPageReorder`）並送 `reorderPagesRequested` → `PDFProgramController::reorderPages` 驗證完整 permutation → `PDFDocumentModifier`／`builder->getPages()`／`setPages()` → `markReset` → `onDocumentModified(Reset | PreserveUndoRedo)`，沿用既有 `PDFUndoRedoManager`。只重排 page tree 的 page reference，不複製頁面物件、不改 writer；巢狀 page tree 會先用既有 `flattenPageTree()`（把繼承的 MediaBox／CropBox／Resources／Rotate 寫進頁面）。
- IconMode 的標準 model drag/drop 在探測中沒有給出可用的插入行（見 `docs/page-reorder-v8.md`），所以由 `PDFThumbnailsListView` 依 `visualRect()` 計算插入點（左／上半＝前、右／下半＝後、空白處＝文件最後），不寫死像素。view 只在自己的 drag 進行中接受 drop，檔案拖到側邊欄仍交給主視窗開檔。
- 新檔：`Pdf4QtLibGui/pdfpagereorder.*`、`pdfthumbnailslistview.*`、`docs/page-reorder-v8.md`。改動：`pdfsidebarwidget.*`（含 `.ui` 把縮圖 view 升級成自訂 widget；`setDocument` 對「保留 undo 的編輯」改為停留在目前側邊欄頁）、`pdfprogramcontroller.*`、`pdfeditormainwindow.cpp`、`pdfitemmodels.*`（縮圖 model 加 `ItemIsDragEnabled`）。沒有新增 tr() 字串、沒有新增或升級 dependency。

## v8 驗證證據（本機 Release）

- `UnitTestsViewer`（offscreen）新增 `pageReorderOrderMath`（18 列）、`pageReorderInsertionGeometry`、`thumbnailReorderWorkflow`（單頁前／後、連續多頁前／後、Ctrl 不連續、Shift 範圍、第一頁前、最後一頁後與空白處、自身區域 no-op、invalid／duplicate／missing 被拒、Undo／Redo、選取與目前頁面）、`reorderPreservesContentAfterSave`（含註解、旋轉頁、AcroForm 欄位、文字層：重排 → Save As → 重開後逐項確認）、`reorderFlattensNestedPageTree`、`viewerThumbnailsAreReadOnly`：全 PASS。
- 真實 Windows 平台滑鼠拖曳 `nativeThumbnailDragSmoke`（`FAMILYPDF_NATIVE_DRAG_SMOKE=1`、`QT_QPA_PLATFORM=windows`，會移動實體游標、需視窗在最上層）：單頁、多選、Undo、Redo PASS。跑的時候別動滑鼠；偶爾因人為移動或視窗被遮住而 drag 沒落到 view，測試會重試。
- 回歸 smoke PASS：`thumbnailSelectionAndPageManagement`（選取、刪除、旋轉、提取）、`printAndExportEntriesAreAvailable`、`printDialogOptionsAndCancel`、`exportImagesDialogWorkflow`、`exportSelectionAsImageWorkflow`、`filledFormPrintsAndExports`、`menuActionsOperateOnTheDocument`、`UnitTestsBookmarks`、`UnitTestsDocumentEdit`。未重跑本機完整 CTest 與 `dist/FamilyPDF`（依規定交給 CI）。
- Ubuntu CI 根因：`Pdf4QtLibGui` 只對使用者宣告 `INTERFACE` include 目錄，自己的 target 沒有自己的原始碼目錄；MSVC 找 `#include "..."` 時會連同「正在被 include 的每個檔案」所在目錄一起找，所以 Windows 找得到，GCC 只找 include 檔自己的目錄（autogen/include）與 `-I`，找不到。修法：`Pdf4QtLibGui/CMakeLists.txt` 的 `target_include_directories(... INTERFACE ...)` 改 `PUBLIC`。之後 `.ui` 若再引用自訂 widget 標頭，靠這行即可。驗證：用 target 的實際編譯指令、把只含 `ui_pdfsidebarwidget.h` 的 TU 放在原始碼目錄外（等同 GCC 的搜尋規則），修前 `C1083` 找不到 header、修後通過。
- 本機 build：新增檔案需要重新 configure；`VCPKG_MANIFEST_INSTALL=OFF` 已固定所以 vcpkg 沒被動到，重新產生後把 `rules.ninja` 第 17 行的 `msvc_deps_prefix` 還原成 `注意: 包含檔案:`（`ninja -t deps` 確認新 obj 有 #deps）。

## v7 已交付內容

- Viewer／Editor 共用：File → Print（含預覽）、File → Export Page(s) as Images…、縮圖右鍵「列印選取的頁面／匯出選取的頁面為圖片」、文字選取後右鍵「Export Selection as Image…」。Editor 原有進階「Render to Images…」不變。
- 列印：`PDFPageOutput::print`（`Pdf4QtLibGui/pdfpageoutput.*`）沿用向量列印管線逐頁輸出，不預先 rasterize、不預先處理未列印頁；註解以 `PDFAnnotationManager`（Print target）繪製。支援全部／目前頁／縮圖選取／範圍（`1-3,8,10-12`，依文件順序、不重複）、自動／直向／橫向（逐張依頁面方向切換）、符合可列印區域／實際大小、印表機、紙張、色彩、份數／逐份、雙面（只在 `QPrinterInfo` 回報支援時提供，不模擬）。無原生份數的引擎（PDF 輸出）由程式重複頁面。取消在下一張前生效並中止工作、移除指定輸出檔。
- 預覽：列印對話框內的單張預覽（`pdfprintdialog.*`），背景只渲染正在看的那一頁；未用 `QPrintPreviewDialog`，因為它開啟時會渲染整份工作的所有頁面。
- 圖片匯出：`PDFPageImageExporter` 沿用 `PDFRasterizerPool`／`PDFRenderer::compile`／`PDFAnnotationManager`，輸出 PNG／JPEG（預設 PNG、150 DPI、JPEG 品質 90；36–1200 DPI；單張上限 1.2 億像素）。白底平面化（rasterizer 對未繪製區域為透明，JPEG 會變黑）、檔名 `<文件>_p<頁碼補零>.<ext>`（選取為 `_selection`）、`QSaveFile` 原子寫入、覆蓋前詢問、取消／部分失敗如實回報「N / M 已儲存」。選取範圍匯出使用文字選取的邊界框（與標記工具同一幾何）。
- 頁面區域沿用畫面規則：以旋轉後 MediaBox 為輸出範圍，內容依 ClipToCropBox 裁切，CropBox 小於 MediaBox 時尺寸仍為 MediaBox。列印／匯出不帶畫面專用模式（反相、灰階、高對比、除錯疊圖、渲染時間）。
- 繁中／簡中翻譯已補（`translations/PDF4QT_zh_TW.ts`、`PDF4QT_zh_CN.ts`）。說明文件：`docs/print-export-v7.md`。
- 未修改 renderer／色彩管理／搜尋／OCR／表單與簽章核心；未新增或升級 dependency。

## v7 驗證與交付證據

- 新測試 `UnitTestsPrintExport`（35 項，offscreen 33 通過＋2 項因無 Windows 字型／平台外掛而 SKIP；`QT_QPA_PLATFORM=windows` 下 35/35，含 Microsoft Print to PDF 驅動與中文字型）與 `UnitTestsViewer` 新增 6 項（入口／縮圖選單、列印對話框、匯出對話框、選取匯出、填寫表單列印與匯出、1,200 頁量測）。完整 CTest 10/10 通過。
- 效能（本機 Release）：1,200 頁文件開列印對話框至預覽第一頁約 280 ms；第 600 頁 1 頁 PNG 29 ms；10 頁 PNG 84 ms；列印第 600 頁 14 ms；A4 PNG 600 DPI 約 680 ms。只輸出少數頁時時間不隨頁數增加，不需優化。
- `dist/FamilyPDF` 只更新自家二進位（exe、`Pdf4QtLib*.dll`、`pdfplugins`、zh qm）；第三方 DLL 未動。Viewer／Editor clean-PATH smoke 通過。

## Git 狀態

- v9：branch `feature/merge-pdfs-v9`（本機驗證完成；commit／push／PR 狀態以 `git log`／GitHub 為準，見最終回報）。
- v8：PR #17 已 squash merge（`7cfa289b`）。
- v7：PR #15 已 squash merge 為 `60be9699`，本機 `main` 已同步，feature branch 已刪除（本機與遠端）。本機 `gh` 仍未登入，無法自行建立 PR 或查 CI；merge 由使用者在 GitHub 完成。

## 限制與接手注意事項

- 列印在 GUI 執行緒逐頁進行（`QPainter` 不能跨執行緒），取消只在張與張之間檢查；預覽是單張光柵近似，實際輸出為向量。
- `Microsoft Print to PDF` 會跳出存檔對話框；Qt 對 `.pdf` 輸出檔名會自己寫 PDF 而不走驅動，驅動路徑測試用非 `.pdf` 副檔名。
- 區域匯出只支援文字選取；高 DPI 區域匯出會先渲染整頁再裁切（受 1.2 億像素上限保護）。
- 原有限制仍適用：密碼欄位不保存、`/Tabs /S` 以 annotation order 代替、現有 full-rewrite save 不保留已簽署 PDF 的原簽章有效性（見 `docs/forms-signatures-v6.md`）。
- **本機建置環境（重要）**：系統為 zh-TW，MSVC 的 `/showIncludes` 前綴為中文，CMake 偵測成亂碼，導致 Ninja 不追蹤標頭（`ninja -t deps` 為 `#deps 0`）。本輪已把 `build/phase0-upstream-release/CMakeFiles/rules.ninja` 的 `msvc_deps_prefix` 改為 `注意: 包含檔案:` 並完整重建；CMake 重新產生時會被還原，需重做。另外重新 configure 曾觸發 vcpkg 移除並重建失敗（`vcpkg_installed` 被清空），已由 `FamilyPDF-tools/binary-cache` 的 zip 還原，並以 `-DVCPKG_MANIFEST_INSTALL=OFF` 固定；build 目錄內第三方 DLL 因此與 `dist` 的 hash 不同（同版本）。細節見使用者記憶 `familypdf-local-build-quirks`。
- v9 新增原始檔後又 re-configure 過一次，已把備份的 `rules.ninja` 第 17 行貼回；注意新增／修改 `.ui`、CMakeLists 會再度觸發 re-run 並讓該行變亂碼，build 中途 re-run 時那一輪編出的 obj 會是 `#deps 0`（改 `pdfdocumentmerger.h`、`pdfmergepdfsdialog.h` 時要 touch 引用它的 .cpp）。PowerShell 工具會擋 `Remove-Item`，刪檔用 `[IO.File]::Delete`。
- 工具位於同層 `FamilyPDF-tools`；既有 build 為 `build/phase0-upstream-release`。本機不要為文件更新或 CI 等待重跑 Full。

## 需要深入時再讀

- v9 行為、上游限制分類與測試：`docs/merge-pdfs-v9.md`；引擎 `Pdf4QtLibCore/sources/pdfdocumentmerger.*`，對話框 `Pdf4QtLibGui/pdfmergepdfsdialog.*`，流程 `pdfprogramcontroller.cpp` 的 `mergePdfs`，測試 `UnitTests/tst_mergepdfstest.cpp` 與 `tst_viewercontextmenutest.cpp` 的 `mergePdfs*`。
- v8 行為與資料流：`docs/page-reorder-v8.md`；核心 `Pdf4QtLibGui/pdfpagereorder.*`、`pdfthumbnailslistview.*`，控制器 `pdfprogramcontroller.cpp` 的 `reorderPages`；測試見 `tst_viewercontextmenutest.cpp` 的 `pageReorder*`、`thumbnailReorderWorkflow`、`reorderPreservesContentAfterSave`、`reorderFlattensNestedPageTree`、`nativeThumbnailDragSmoke`。
- v7 行為與限制：`docs/print-export-v7.md`；核心 `Pdf4QtLibGui/pdfpageoutput.*`，對話框 `pdfprintdialog.*`、`pdfexportimagesdialog.*`，流程 `pdfprogramcontroller.cpp` 的 `runPrintWorkflow`／`runExportImagesWorkflow`／`exportSelectionAsImage`。
- v7 回歸：`UnitTests/tst_printexporttest.cpp`；`tst_viewercontextmenutest.cpp` 的 `printAndExportEntriesAreAvailable`、`printDialogOptionsAndCancel`、`exportImagesDialogWorkflow`、`exportSelectionAsImageWorkflow`、`filledFormPrintsAndExports`、`printExportLargeDocumentBenchmark`。
- v6 行為與限制：`docs/forms-signatures-v6.md`（表單核心 `pdfform.*`、儲存／UndoRedo `pdfprogramcontroller.cpp`）。
- 其他工作區背景：`docs/WORKSPACE-HANDOFF.md`，視新任務需要讀取；舊快照不能取代本文件或現場狀態。
