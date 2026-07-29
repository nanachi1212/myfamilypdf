# Windows 可攜式包使用方式

目前可攜式包：`dist/FamilyPDF-windows-x64.zip`

1. 解壓縮整個 ZIP，不要只複製單一 `.exe`。
2. 執行 `Pdf4QtViewer.exe` 開啟閱讀器；需要編輯 PDF 時執行 `Pdf4QtEditor.exe`。
3. PDF 合併／拆分可使用 `Pdf4QtPageMaster.exe`，命令列批次處理可使用 `PdfTool.exe`。
4. `translations/PDF4QT_zh_TW.qm` 與 `translations/PDF4QT_zh_CN.qm` 已隨包提供；程式會依 Windows／Qt 語言設定載入。
5. OCR 可把 PDF 拖曳到 `FamilyPDF-OCR.cmd`；內含繁中、簡中與英文語言資料。
6. `vc_redist.x64.exe` 隨包提供，若乾淨 Windows 電腦無法啟動，可先安裝它。

## 合併指定頁面

在 `Pdf4QtPageMaster.exe` 匯入 PDF 後：

1. 使用 `Select Odd` 只選單數頁，或 `Select Even` 只選雙數頁。
2. 也可使用 `Select Page Range...`，輸入 `1-3,8,10-12`。
3. 執行 `United Document...`；有選取時只合併選取頁，未選取時合併全部頁面。

## 產物驗證

2026-07-29 的可攜式 ZIP：

- 大小：96,063,098 bytes（約 91.6 MiB）。
- SHA-256：`02B69D694F7C09B0369E4F8A70576B88791C4B78A647F030D3F116C950F59096`。
- 三組單元測試、Tesseract 語言載入及封裝版 OCR 皆以 exit code `0` 通過。

這是完整可攜式包；若希望有開始功能表捷徑與解除安裝功能，請改用 `FamilyPDF-Setup-x64.exe`。
