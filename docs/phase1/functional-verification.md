# Phase 1 功能驗證結果

更新日期：2026-07-29

## 可攜式包 smoke test

- `Pdf4QtViewer.exe`：啟動後維持執行 8 秒，未立即退出。
- `UnitTests.exe`：exit code `0`。
- `UnitTestsImageOptimizer.exe`：exit code `0`。
- `UnitTestsFontEncoding.exe`：exit code `0`。
- 可攜式包包含 `translations/PDF4QT_zh_TW.qm` 與 `translations/PDF4QT_zh_CN.qm`。
- PageMaster 合併流程已改為：有選取頁面時只合併選取內容；無選取時合併全部內容。可先使用 `Select Even`、`Select Odd` 或 `Select Page Range...` 建立選取。
- 精簡封裝後不含 Qt 除錯 DLL，三組單元測試仍全部以 exit code `0` 通過。
- 精簡可攜式 ZIP 約 84.6 MB；若 `windeployqt` 全部正常完成則約 64.4 MB，封裝腳本保留完整 release Qt fallback，避免工具偶發失敗造成缺檔。

## 命令列 PDF 流程

使用可攜式包內的 `PdfTool.exe` 與基底測試 PDF：

- `help`：成功列出 `unite`、`separate`、`fetch-text` 等命令。
- `unite a.pdf b.pdf merged.pdf`：exit code `0`，成功產生合併檔。
- `info merged.pdf`：exit code `0`，合併檔為 58 頁。
- `separate merged.pdf split-%.pdf`：exit code `0`，成功產生 58 個分頁 PDF。

大型檔案基準：

- 以 5 份 58 頁 PDF 合併成 290 頁：exit code `0`，約 0.50 秒。
- `info` 讀取 290 頁 PDF：exit code `0`，約 0.35 秒，檔案大小 867,678 bytes。
- 用可攜式包實際開啟 290 頁 PDF 並維持 8 秒：
  - `Pdf4QtViewer.exe`：仍正常執行，工作集約 18.5 MB。
  - `Pdf4QtPageMaster.exe`：仍正常執行，工作集約 31.1 MB。

## 安裝檔

- 已使用 Inno Setup 7.0.2 成功編譯 `dist\FamilyPDF-Setup-x64.exe`。
- 編譯時已載入繁體中文、簡體中文與英文安裝語系。
- Inno Setup 下載檔已先通過 Windows Authenticode 驗證，簽章有效且發行者為 `Pyrsys B.V.`。
- 目前已完成不含 OCR 的安裝器建置驗證；含 OCR 的最終安裝器會在 Tesseract 相依套件完成後重建。

驗證素材位於本機 `build/phase1-verification/`，未納入 Git；可重新產生，不影響原始 PDF。

## 尚未由自動測試證明的項目

- GUI 中實際建立彩色標記、文字註解與書籤後重新開檔保存。
- 繁體中文與簡體中文 Windows locale 的實機顯示。
- 數百頁大型 PDF 的實際人工快速翻頁體感與長時間壓力測試。
- OCR 的最終文字辨識正確性與封裝。
