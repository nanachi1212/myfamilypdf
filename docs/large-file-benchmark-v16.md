# 大檔效能基準與儲存策略決定 v16

更新日期：2026-10-08。用本機既有的 1,160 頁、32 MB 文件（`build/release-validation/large-1160-pages.pdf`，不入庫）量測，決定是否要做 PDF incremental update（增量儲存）。

## 讀取（`UnitTestsViewer largePdfReadingBenchmark`，Release、offscreen）

| 項目 | 時間 |
|---|---:|
| 開檔完成（1,160 頁） | 730 ms |
| 首頁可見 | 778 ms |
| 首張縮圖 | 779 ms |
| 跳第 20／100／200／364 頁 | 28／37／34／34 ms |
| 冷 cache 連跳三頁 | 31 ms |
| 回到已渲染首頁 | 0 ms |

## 寫入（同一份文件）

| 項目 | 時間 |
|---|---:|
| 整檔重寫（`PdfTool optimize --opt-remove-null`，與 Save 同一個 `PDFDocumentWriter`） | 353 ms |
| 安全儲存的整檔 SHA-256 | 81 ms |

## 決定

- **不做 incremental save。** 32 MB 整檔重寫不到 0.4 秒，安全儲存的雜湊不到 0.1 秒；就算檔案大十倍也在幾秒內，而增量儲存要改 writer、安全儲存與簽章相關核心，風險遠高於收益。
- 若未來實際檔案（例如數百 MB 掃描檔）存檔明顯卡頓，再以同樣方法量測後決定；量測指令見本文件與 `docs/qa/large-pdf-reading-v2.md`。
