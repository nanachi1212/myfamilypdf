# Windows 可攜式包使用方式

目前可攜式包：`dist/FamilyPDF-windows-x64.zip`

1. 解壓縮整個 ZIP，不要只複製單一 `.exe`。
2. 執行 `Pdf4QtViewer.exe` 開啟閱讀器；需要編輯 PDF 時執行 `Pdf4QtEditor.exe`。
3. PDF 合併／拆分可使用 `Pdf4QtPageMaster.exe`，命令列批次處理可使用 `PdfTool.exe`。
4. `translations/PDF4QT_zh_TW.qm` 與 `translations/PDF4QT_zh_CN.qm` 已隨包提供；程式會依 Windows／Qt 語言設定載入。
5. `vc_redist.x64.exe` 隨包提供，若乾淨 Windows 電腦無法啟動，可先安裝它。

這是目前的可攜式測試包，不是正式簽章安裝程式；OCR 與家庭版預設介面仍在後續階段。
