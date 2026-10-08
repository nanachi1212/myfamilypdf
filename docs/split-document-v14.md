# 拆分文件 v14

更新日期：2026-10-08。Viewer／Editor 的 File 選單新增 **Split Document...**，把目前文件寫成多個新 PDF；目前文件不變、不進 Undo。沒有新增或升級 dependency。

## 功能

- 模式：每頁一檔、每 N 頁一檔、或「新檔案從第 5,12 頁開始」（頁碼解析重用 v9 `PDFDocumentMerger::parsePageList`，重複與順序會整理）。
- 輸出：指定資料夾（預設來源資料夾）＋基本檔名（預設來源檔名），檔名 `<基本檔名>_p<起>-<迄>.pdf`，單頁為 `_p5.pdf`。
- 保護：輸出檔不得等於目前文件；已存在的檔案先列出並詢問（預設「否」）；每個檔案以 `QSaveFile` 原子寫入，中途失敗時回報「已儲存 N / M」。
- 只有一頁、或設定只會產生一個檔案時不執行。

## 實作

- `PDFProgramController::writePagesToFile`：從 `extractPages` 抽出的靜態 helper（`PDFDocumentManipulator` `NoOutline` + `PDFDocumentWriter` + `QSaveFile`），提取與拆分共用。
- `PDFProgramController::splitDocument` + 對話框 `Pdf4QtLibGui/pdfsplitdocumentdialog.*`（純程式碼，`splitEveryN`／`splitAtPages` 為靜態純函式）。

## 限制

- 輸出不保留書籤與文件層級物件（與提取相同）。
- 在 GUI 執行緒逐檔寫入，沒有進度條；一般家用文件秒級完成。

## 測試

`UnitTestsViewer::splitDocumentWorkflow`：選單 action 存在；每 2 頁拆分三頁文件產生 `part_p1-2.pdf`、`part_p3.pdf` 並可重開、目前文件不變；已存在檔案的詢問預設「否」且檔案位元組不變；拆分數學（每 N 頁、指定起始頁、去重、空輸入）。
