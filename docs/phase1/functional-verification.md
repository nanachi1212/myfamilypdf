# Phase 1 功能驗證結果

更新日期：2026-07-29

## 可攜式包 smoke test

- `Pdf4QtViewer.exe`：啟動後維持執行 8 秒，未立即退出。
- `UnitTests.exe`：exit code `0`。
- `UnitTestsImageOptimizer.exe`：exit code `0`。
- `UnitTestsFontEncoding.exe`：exit code `0`。

## 命令列 PDF 流程

使用可攜式包內的 `PdfTool.exe` 與基底測試 PDF：

- `help`：成功列出 `unite`、`separate`、`fetch-text` 等命令。
- `unite a.pdf b.pdf merged.pdf`：exit code `0`，成功產生合併檔。
- `info merged.pdf`：exit code `0`，合併檔為 58 頁。
- `separate merged.pdf split-%.pdf`：exit code `0`，成功產生 58 個分頁 PDF。

大型檔案基準：

- 以 5 份 58 頁 PDF 合併成 290 頁：exit code `0`，約 0.50 秒。
- `info` 讀取 290 頁 PDF：exit code `0`，約 0.35 秒，檔案大小 867,678 bytes。

驗證素材位於本機 `build/phase1-verification/`，未納入 Git；可重新產生，不影響原始 PDF。

## 尚未由自動測試證明的項目

- GUI 中實際建立彩色標記、文字註解與書籤後重新開檔保存。
- 繁體中文與簡體中文 Windows locale 的實機顯示。
- 數百頁大型 PDF 的記憶體與翻頁效能。
- OCR。
