# Phase 1 功能盤點

更新日期：2026-07-30

## 已由 PDF4QT 基底提供的能力

| 需求 | 原始碼證據 | 判定 |
|---|---|---|
| 書籤 | `Pdf4QtLibGui/pdfbookmarkmanager.*`、`pdfbookmarkui.*` | 已有書籤管理與側欄 UI |
| 彩色劃線／標記 | `Pdf4QtLibCore/sources/pdfannotation.*`、`pdfdocumentbuilder.h`；GUI 有 `highlight.svg`、`underline.svg`、`strikeout.svg`、`squiggly.svg` | 已有文字標記模型與工具 |
| 打字／文字註解 | `Pdf4QtLibWidgets/sources/pdfadvancedtools.*` 的 free text tool；`pdfdocumentbuilder.h` 的 free text annotation | 已有基本文字註解能力 |
| 註解側欄 | `Pdf4QtLibGui/pdfsidebarwidget.*`、`resources/sidebar-annotations.svg` | 已有註解檢視入口 |
| 填寫／建立表單 | `Pdf4QtLibWidgets/sources/pdfwidgetformmanager.*`、`Pdf4QtLibCore/sources/pdfform.*`、`Pdf4QtEditorPlugins/FormPlugin` | 既有 AcroForm 可互動；已能建立文字框、核取方塊、單選按鈕群組、下拉選單與清單 |
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
- `Forms` 選單可拖曳建立標準 AcroForm 文字框、核取方塊、單選按鈕群組、下拉選單與清單，設定常用欄位屬性、清單多選、切換欄位反白及重設預設值。
- `Document Edit` 選單可依頁碼範圍及單雙數加入文字浮水印、純色／圖片背景、調整頁面尺寸與裁切框、縮放內容與 annotation，以及向左／向右旋轉。
- `Office Export` 選單可將全部或 `1-3,5` 指定頁碼匯出成 DOCX／XLSX；helper 自帶 Python runtime，使用端無須安裝 Python。
- 安裝包內六個正式功能插件（`Document Edit`、`Editor`、`Forms`、`FamilyPDF Office Export`、`Redact`、`Signature`）會在首次啟動及舊 PDF4QT 設定升級時自動啟用，使用者不必進入插件設定。
- 已產生基礎可攜式 ZIP，內含 Viewer、Editor、PageMaster、PdfTool 與 Qt runtime；OCR 維持獨立安裝包。
- Viewer／Editor 可一次接收多個 PDF，為每份文件建立獨立視窗，並以同步文件分頁切換；工作階段可於重啟後還原。
- 已用 1,160 頁 PDF 與兩份一般 PDF 同時驗證 Viewer／Editor，三份文件均寫入工作階段且程式保持回應。
- 已以安裝後的程式驗證三組單元測試、合併、拆分、OCR 與書籤重啟保存。

## 合併頁面篩選（已加入家庭版流程）

合併前可先在 PageMaster 使用：

- `Select Even`：選取目前工作區中的偶數頁。
- `Select Odd`：選取目前工作區中的單數頁。
- `Select Page Range...`：輸入例如 `1-3, 8, 10-12`，可按工作區順序或目前來源 PDF 原始頁碼選取。

再執行 `United Document...` 時，若有選取頁面，只會合併選取內容；沒有選取時則合併全部內容。

## 已知限制

- 五種 AcroForm 欄位已完成建立 UI 與 parser round-trip 自動測試；仍待外部 GUI 閱讀器人工巡覽。
- PDF 轉 DOCX／XLSX 已完成文字層、基本樣式、表格與逐行 fallback；複雜版面可能需要人工整理。
- 進階內容編輯、浮水印、背景與頁面幾何工具已完成自動測試；仍待繁體／簡體 Windows 的 GUI 人工巡覽。
- 尚未在兩台分別使用繁體中文與簡體中文 Windows 的實機上逐頁人工巡覽；目前證據是雙語 `.qm`、雙語安裝語系與本機 Windows 執行驗證。
- 自製安裝程式尚未使用商業程式碼簽章憑證；Windows SmartScreen 可能顯示未知發行者。
- 數百頁測試證明可開啟與保持回應，不等同長時間人工快速翻頁壓力測試。
