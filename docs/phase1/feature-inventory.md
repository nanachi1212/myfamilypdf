# Phase 1 功能盤點

更新日期：2026-07-29

## 已由 PDF4QT 基底提供的能力

| 需求 | 原始碼證據 | 判定 |
|---|---|---|
| 書籤 | `Pdf4QtLibGui/pdfbookmarkmanager.*`、`pdfbookmarkui.*` | 已有書籤管理與側欄 UI |
| 彩色劃線／標記 | `Pdf4QtLibCore/sources/pdfannotation.*`、`pdfdocumentbuilder.h`；GUI 有 `highlight.svg`、`underline.svg`、`strikeout.svg`、`squiggly.svg` | 已有文字標記模型與工具 |
| 打字／文字註解 | `Pdf4QtLibWidgets/sources/pdfadvancedtools.*` 的 free text tool；`pdfdocumentbuilder.h` 的 free text annotation | 已有基本文字註解能力 |
| 註解側欄 | `Pdf4QtLibGui/pdfsidebarwidget.*`、`resources/sidebar-annotations.svg` | 已有註解檢視入口 |
| 合併 PDF | `Pdf4QtPageMaster` 的 United Document 操作 | 已有頁面組合流程 |
| 拆分 PDF | `Pdf4QtPageMaster/mainwindow.cpp` 的 Split 操作 | 支援每頁、每 N 頁、指定頁碼、頂層書籤與近似檔案大小 |

## 尚需驗證或客製

- 尚未在實際 Windows GUI 中逐項點擊驗證繁體中文與簡體中文介面。
- 尚未用數百頁以上 PDF 做開啟時間、記憶體與翻頁測試。
- OCR 目前沒有在本基底找到 Tesseract／Windows OCR 整合；需另行設計與評估。
- 尚未建立家庭版預設工具列、快捷鍵與中文翻譯調整。
- 尚未建立正式安裝程式；目前交付形式是可攜式 ZIP。

## 合併頁面篩選（已加入家庭版流程）

合併前可先在 PageMaster 使用：

- `Select Even`：選取目前工作區中的偶數頁。
- `Select Odd`：選取目前工作區中的單數頁。
- `Select Page Range...`：輸入例如 `1-3, 8, 10-12`，可按工作區順序或目前來源 PDF 原始頁碼選取。

再執行 `United Document...` 時，若有選取頁面，只會合併選取內容；沒有選取時則合併全部內容。

## 下一個實作順序

1. 用可攜式包啟動 Viewer／Editor，完成基本 GUI smoke test。
2. 建立最小測試 PDF，驗證書籤、標記、文字註解與合併／拆分輸出。
3. 用大型 PDF 做效能記錄。
4. OCR 架構設計前切換至 Sol，再決定 Windows OCR 或 Tesseract 路線。
