# 插入頁面與拆分文件 v10

更新日期：2026-10-08。Editor 補上 PageMaster 以外唯一缺的頁面操作：把別的 PDF 插進目前文件、插入空白頁、把目前文件拆成多個檔案。沒有新增或升級 dependency。

## 功能

- **File → Insert Pages from File...**（只有 Editor）：選一份 PDF → 對話框輸入頁面（`all` 或 `1-3,8`，順序照打，可重複）與位置（某頁之前／之後，預設目前頁之後）→ 插入目前文件，可 Undo／Redo。插入前用 v9 的 `PDFDocumentMerger::analyze` 檢查：來源或目前文件權限不允許複製／組合、沒有頁面＝阻止；加密（結果不加密）、數位簽章（結果簽章無效）、XFA、具名目的地、同名表單欄位＝警告後可繼續。
- **File → Insert Blank Page**（只有 Editor）：在目前頁之後插入一張與目前頁相同尺寸的空白頁，可 Undo／Redo；要放別處用 v8 縮圖拖曳。
- **File → Split Document...**（Viewer／Editor）：每頁一檔、每 N 頁一檔、或「新檔案從第 5,12 頁開始」。輸出到指定資料夾，檔名 `<基本檔名>_p<起>-<迄>.pdf`（單頁 `_p5.pdf`），覆蓋前詢問，輸出不得等於目前文件；目前文件不變、不進 Undo。

## 實作

- 引擎：`PDFDocumentMerger::mergeToDocument(entries, PDFDocument*)` 從 `mergeToFileImpl` 抽出（記憶體合併，不寫檔）。同一個 `PDFDocument` 物件在多列只當一個來源（`std::map<const PDFDocument*, int>`），所以「前段／插入／後段」三列不會把目前文件複製兩次；同一檔案開兩次仍是兩個獨立來源（v9 行為不變）。`mergeToFileImpl` 改為呼叫它再寫檔。
- 插入：`PDFProgramController::insertPagesFromFile` 組三列 entries → `mergeToDocument` → 新 `PDFDocumentPointer` 以 `Reset | PreserveUndoRedo` 交給既有 `onDocumentModified`，與 v8 重排同一條路。整份文件會被重新組裝（物件重新編號）；書籤／表單／optional content 沿用 v9 的 Join／attach 行為。
- 空白頁：`PDFDocumentModifier` + 既有 `PDFDocumentBuilder::appendPage(mediaBox)`，再用 `setPages` 把新頁移到目前頁之後；巢狀 page tree 先 `flattenPageTree`。
- 拆分：`PDFProgramController::writePagesToFile`（從 `extractPages` 抽出的靜態 helper，`QSaveFile` 原子寫入、`NoOutline`）逐部分呼叫。拆分點解析重用 `PDFDocumentMerger::parsePageList`。
- 對話框：`Pdf4QtLibGui/pdfpageinsertsplitdialogs.*`（`PDFInsertPagesDialog`、`PDFSplitDocumentDialog`，純程式碼、無 `.ui`）。

## 已知限制

- 插入後整份文件重新組裝：與 v9 合併相同的 A 類限制（tagged structure tree／文件動作／threads 不保留、連到未插入頁面的連結失效）。
- 空白頁固定插在目前頁之後、尺寸同目前頁、不帶旋轉。
- 拆分輸出不保留書籤（與提取相同）。

## 測試

- `UnitTestsMergePdfs::mergeToDocumentSharesSource`：A(第1頁)+B(2,1)+A(2,3) → 順序、5 個獨立頁面物件、A 的表單欄位只出現一次、寫出後讀回。
- `UnitTestsViewer::insertAndSplitWorkflow`：三個選單 action 存在；空白頁位置／尺寸／無內容與 Undo／Redo；從檔案插入 `2,1` 到第 2 頁之後的文字層順序與 Undo／Redo；每 2 頁拆分產生 `part_p1-2.pdf`、`part_p3.pdf` 且目前文件不變；拆分數學（每 N 頁、指定起始頁、去重）。
