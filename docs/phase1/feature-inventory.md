# Phase 1 功能盤點

更新日期：2026-07-29

## 已由 PDF4QT 基底提供的能力

| 需求 | 原始碼證據 | 判定 |
|---|---|---|
| 書籤 | `Pdf4QtLibGui/pdfbookmarkmanager.*`、`pdfbookmarkui.*` | 已有書籤管理與側欄 UI |
| 彩色劃線／標記 | `Pdf4QtLibCore/sources/pdfannotation.*`、`pdfdocumentbuilder.h`；GUI 有 `highlight.svg`、`underline.svg`、`strikeout.svg`、`squiggly.svg` | 已有文字標記模型與工具 |
| 打字／文字註解 | `Pdf4QtLibWidgets/sources/pdfadvancedtools.*` 的 free text tool；`pdfdocumentbuilder.h` 的 free text annotation | 已有基本文字註解能力 |
| 註解側欄 | `Pdf4QtLibGui/pdfsidebarwidget.*`、`resources/sidebar-annotations.svg` | 已有註解檢視入口 |
| 填寫／建立表單 | `Pdf4QtLibWidgets/sources/pdfwidgetformmanager.*`、`Pdf4QtLibCore/sources/pdfform.*`、`Pdf4QtEditorPlugins/FormPlugin` | 既有 AcroForm 可互動；已能建立文字框與核取方塊，單選與選單欄位仍在開發 |
| 直接內容編輯 | `Pdf4QtEditorPlugins/EditorPlugin` | 已納入 release target 與可攜包，可新增文字／圖形／SVG、刪除及復原／重做 |
| 永久遮蔽 | `Pdf4QtEditorPlugins/RedactPlugin` | 已納入 release target 與可攜包 |
| 電子／數位簽章 | `Pdf4QtEditorPlugins/SignaturePlugin` | 已納入 release target 與可攜包 |
| 合併 PDF | `Pdf4QtPageMaster` 的 United Document 操作 | 已有頁面組合流程 |
| 拆分 PDF | `Pdf4QtPageMaster/mainwindow.cpp` 的 Split 操作 | 支援每頁、每 N 頁、指定頁碼、頂層書籤與近似檔案大小 |

## FamilyPDF 已完成的客製

- Viewer 與 Editor 共用 `%LOCALAPPDATA%\FamilyPDF\bookmarks.json`，書籤可跨程式及重啟保存。
- 已加入 Tesseract 5 OCR、`chi_tra`、`chi_sim`、`eng` 語言資料與自動下載／封裝流程。
- 已建立含繁體中文、簡體中文及英文介面的 Inno Setup 安裝程式。
- release 建置與可攜包已包含 `EditorPlugin.dll`、`RedactPlugin.dll`、`SignaturePlugin.dll`、`FormPlugin.dll`，並有自動打包檢查。
- `Forms` 選單可拖曳建立標準 AcroForm 文字框與核取方塊，設定常用欄位屬性、切換欄位反白及重設預設值。
- 已產生完整可攜式 ZIP，內含 Viewer、Editor、PageMaster、PdfTool、Qt runtime 與 OCR。
- 已用 290 頁 PDF 驗證 Viewer 與 PageMaster 能開啟並保持回應。
- 已以安裝後的程式驗證三組單元測試、合併、拆分、OCR 與書籤重啟保存。

## 合併頁面篩選（已加入家庭版流程）

合併前可先在 PageMaster 使用：

- `Select Even`：選取目前工作區中的偶數頁。
- `Select Odd`：選取目前工作區中的單數頁。
- `Select Page Range...`：輸入例如 `1-3, 8, 10-12`，可按工作區順序或目前來源 PDF 原始頁碼選取。

再執行 `United Document...` 時，若有選取頁面，只會合併選取內容；沒有選取時則合併全部內容。

## 已知限制

- AcroForm 文字框與核取方塊 UI 已完成；單選按鈕、下拉選單與清單欄位仍在開發。
- 尚未提供 PDF 轉 DOCX／XLSX；規劃為選裝 Office Export 外掛。
- 進階內容編輯第一階段沿用 PDF4QT 外掛介面，浮水印、背景與統一頁面幾何工具仍在開發。
- 尚未在兩台分別使用繁體中文與簡體中文 Windows 的實機上逐頁人工巡覽；目前證據是雙語 `.qm`、雙語安裝語系與本機 Windows 執行驗證。
- 自製安裝程式尚未使用商業程式碼簽章憑證；Windows SmartScreen 可能顯示未知發行者。
- 數百頁測試證明可開啟與保持回應，不等同長時間人工快速翻頁壓力測試。
