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
- [x] 一鍵完整安裝程式的完整／精簡元件選擇均隔離安裝成功；完整模式通過繁簡中 OCR，精簡模式不含 OCR。
- [x] 一鍵完整安裝程式的繁中／簡中 GUI 均正確顯示三種安裝類型；完整與精簡模式切換會同步勾選／取消 OCR。
- [x] 繁中／簡中 GUI 正確顯示 Windows PDF 整合且預設勾選；Registry 僅使用 HKCU、受單一安裝 task 控制、命令完整引用路徑及 `%1`，並具解除安裝清理旗標。
- [x] Windows PDF 整合實際安裝與解除安裝 exit code 均為 `0`；Viewer／Editor 可透過完整引用命令開啟含中文與空格的 PDF，解除安裝後 Registry 與安裝檔案均已移除。
- [x] Adobe Acrobat DC 實際開啟、修改並另存 AcroForm；`pypdf` 重讀確認中文欄位名稱、修改值與預設值均保留。
- [x] PDF writer 遇到 CR／LF 的 Unicode 位元組時改用 hexadecimal string，避免外部閱讀器正規化造成中文欄位名稱變異。
- [x] Adobe Acrobat DC 實際開啟、逐頁解析並另存浮水印／背景與頁面幾何 fixture；`pypdf` 結構及 `pypdfium2` 逐頁 RGB 渲染雜湊均保留。
- [x] 完整安裝的 405 個核心檔案與 29 個 OCR 檔案、精簡安裝的 405 個核心檔案，均與目前可攜包逐檔 SHA-256 相同；僅排除安裝版不應攜帶的 `portable.mode`。

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

完整安裝程式：

```powershell
.\scripts\qa\smoke-full-installer.ps1
```

結果會寫入 `build\full-installer-smoke\summary.json`。

Windows PDF 整合安裝／解除安裝：

```powershell
.\scripts\qa\smoke-shell-installation.ps1
```

結果會寫入 `build\shell-installer-smoke-summary.json`。

Adobe Acrobat 表單互通：

```powershell
.\scripts\qa\smoke-acrobat-form-interop.ps1
```

結果會寫入 `build\acrobat-form-interop\summary.json`。

Adobe Acrobat 文件級編輯互通：

```powershell
.\scripts\qa\smoke-acrobat-document-edit.ps1
```

結果會寫入 `build\acrobat-document-edit-interop\summary.json`。

## 發佈產物

- [x] `dist\FamilyPDF-Full-Setup-x64.exe`
- [x] `dist\FamilyPDF-Setup-x64.exe`
- [x] `dist\FamilyPDF-windows-x64.zip`
- [x] `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe`
- [x] `dist\FamilyPDF-OCR-Plugin-windows-x64.zip`
- [x] 主程式與 OCR 外掛分開封裝。
- [x] 同時提供預設含 OCR、可取消 OCR 的一鍵完整安裝程式。
- [x] SHA-256 與檔案大小更新至 `docs\RELEASE-STATUS.md`。

## 仍需人工執行的跨產品／實機驗收

- [ ] 在繁體中文 Windows 實機長時間快速翻閱 1,160 頁 PDF。
- [ ] 在簡體中文 Windows 實機長時間快速翻閱 1,160 頁 PDF。
- [x] 用 Adobe Acrobat DC 填寫並儲存 `dist\qa\form-interop.pdf`，再以獨立 parser 驗證。
- [ ] 由真人巡覽不同內容下的浮水印、背景、裁切及旋轉視覺品質；固定 fixture 已完成 Adobe 另存與逐頁像素等價回歸。
- [ ] 使用 Microsoft Word／Excel 人工巡覽複雜版面匯出結果。
- [ ] 以正式程式碼簽章憑證簽署安裝檔；目前 SmartScreen 可能顯示未知發行者。

這些未勾選項目不代表檔案結構或自動回歸失敗，而是需要人工視覺判斷、不同語系實機或商業簽章憑證才能完成的驗收邊界。Adobe 表單互通已透過官方 IAC 完成；文件級編輯成果仍保留人工視覺巡覽。
