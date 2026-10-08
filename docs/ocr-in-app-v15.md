# OCR 改為程式內執行 v15

更新日期：2026-10-08。Tools → Create Searchable PDF with OCR... 不再彈出黑色命令列視窗，改在程式內以 `QProcess` 執行 `FamilyPDF-OCR.ps1`，有進度條、可取消、完成後詢問是否開啟結果。OCR 腳本、模型與外掛封裝完全不變。

## 行為

- 進度：讀取腳本的 `OCR page N (i/total)...` 行更新進度條與文字；`Rendering...` 顯示「正在轉換頁面影像」。
- 取消：終止 PowerShell 程序。腳本的輸出走 staging，不會留下半成品 PDF；正在跑的那一頁 tesseract 會自行結束。
- 完成：詢問「要現在開啟嗎？」預設是；有開啟文件時以新分頁開啟。
- 失敗：顯示腳本最後 12 行輸出。
- 未安裝外掛（找不到 `FamilyPDF-OCR.cmd`／`.ps1`）：維持原本提示。

## 實作

`PDFProgramController::runOcr(input, output, scriptPath)`：`powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File FamilyPDF-OCR.ps1 -InputPdf ... -OutputPdf ...`，`QProgressDialog` + `QEventLoop`，與 Office Export 外掛同一種作法。`launchOcrPlugin` 只負責檢查與輸出檔名，然後呼叫它。`FamilyPDF-OCR.cmd` 保留給拖放／命令列使用。

## 測試

`UnitTestsViewer::ocrRunsInApp`（Windows）：用假的 `.ps1` 驗證進度（最大值 2、文字「OCR page 2 of 2」）、成功後詢問含輸出路徑、exit 1 時顯示錯誤且無輸出、取消 60 秒腳本在 30 秒內返回且無輸出、腳本不存在時顯示未安裝。
