# FamilyPDF 發佈檢查表

更新日期：2026-07-30

## 自動驗證

- [x] CTest 6/6：核心、圖片最佳化、字型編碼、書籤、表單、文件級編輯。
- [x] Office Export Python 單元測試 7/7。
- [x] 可攜包內六個 Editor 插件與 Office helper 存在。
- [x] DOCX／XLSX 由封裝 helper 產生後可由 `python-docx`／`openpyxl` 重讀。
- [x] 六個正式功能插件可由空白舊插件設定自動啟用。
- [x] 繁中 GUI 顯示三個插件工具列，Office 匯出頁碼對話框可正常開啟。
- [x] `PDF4QT_zh_TW.qm` 與 `PDF4QT_zh_CN.qm` 反向轉換後包含 Office Export 譯文。
- [x] `PdfTool info` 確認大型測試檔為 1,160 頁。
- [x] Viewer／Editor 一次開啟三份 PDF，工作階段各記錄 3 份文件且保持 Responding。
- [x] 主安裝程式隔離靜默安裝 exit code `0`；安裝後插件與 Office helper 回歸通過。
- [x] Microsoft Word 16.0／Excel 16.0 實際唯讀開啟匯出產物；兩頁、多語文字、兩工作表、表格值與合併儲存格驗證通過。
- [x] 1,160 頁 PDF 在繁中／簡中 GUI 完成頁碼欄、文件結尾及縮略圖跳轉，主畫面與縮圖同步且維持 Responding。

執行：

```powershell
.\scripts\qa\run-final-regression.ps1
```

成功結果會寫入 `build\final-regression-<timestamp>\summary.json`。

在已安裝 Microsoft Office 的 Windows 另執行：

```powershell
.\scripts\qa\smoke-microsoft-office.ps1
```

結果會寫入 `build\microsoft-office-smoke\summary.json`。

大檔雙語系存活監測：

```powershell
.\scripts\qa\smoke-large-pdf-locales.ps1
```

結果會寫入 `build\large-pdf-locale-smoke\summary.json`。

## 發佈產物

- [x] `dist\FamilyPDF-Setup-x64.exe`
- [x] `dist\FamilyPDF-windows-x64.zip`
- [x] `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe`
- [x] `dist\FamilyPDF-OCR-Plugin-windows-x64.zip`
- [x] 主程式與 OCR 外掛分開封裝。
- [x] SHA-256 與檔案大小更新至 `docs\RELEASE-STATUS.md`。

## 仍需人工執行的跨產品／實機驗收

- [ ] 在繁體中文 Windows 實機長時間快速翻閱 1,160 頁 PDF。
- [ ] 在簡體中文 Windows 實機長時間快速翻閱 1,160 頁 PDF。
- [ ] 用 Microsoft Edge 或 Adobe Acrobat Reader 填寫並儲存 `dist\qa\form-interop.pdf`。
- [ ] 用 Microsoft Edge 或 Adobe Acrobat Reader 巡覽浮水印、背景、裁切及旋轉結果。
- [ ] 使用 Microsoft Word／Excel 人工巡覽複雜版面匯出結果。
- [ ] 以正式程式碼簽章憑證簽署安裝檔；目前 SmartScreen 可能顯示未知發行者。

這些未勾選項目不代表檔案結構或自動回歸失敗，而是需要人工視覺判斷、不同語系實機或商業簽章憑證才能完成的驗收邊界。瀏覽器控制安全層禁止代理程式載入本機 `file://` PDF，因此 Edge／Adobe 的兩項不得以自動化繞過。
