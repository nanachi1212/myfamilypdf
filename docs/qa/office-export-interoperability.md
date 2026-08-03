# Office 匯出互通驗證

更新日期：2026-07-30

## 已完成

- `pdfplumber` 讀取 PDF 文字層、字型名稱、字級與框線表格。
- DOCX 保留繁體、簡體、英文、基本粗體／斜體與分頁。
- XLSX 每頁建立工作表；表格輸出成儲存格，沒有表格時逐行輸出。
- 頁碼支援全部或 `1-3,5` 格式，會去除重複頁碼並檢查越界。
- 沒有可搜尋文字層時不建立空白 Office 文件，回報 `needs_ocr` 並提示先執行 OCR。
- PyInstaller onedir helper 在移除 Python PATH 後，DOCX 與 XLSX 轉換皆成功。
- 可攜包及主安裝包均包含 `pdfplugins\OfficeExportPlugin.dll` 與
  `office-export\FamilyPDFOfficeExport.exe`。

## 驗證命令

```powershell
.\scripts\office\build-office-export-helper.ps1
.\scripts\qa\smoke-office-export.ps1 -SkipBuild
```

Python 單元測試目前 8 項；C++ CTest 目前 6 組。兩者均已通過。新增測試由真實雙欄 PDF 經封裝 helper 產生 DOCX，確認左右欄分離、欄內順序保留，且有框線表格不會被誤判為雙欄。

## 已知限制

- 這是「可編輯內容匯出」，不是 Word／Excel 的像素級版面複製。
- 一般等寬雙欄會轉為可編輯的無框線兩欄表格；不等寬／三欄以上、跨欄標題、旋轉文字、無框線表格、跨頁表格與特殊字型仍可能需要人工整理。
- 掃描型 PDF 必須先使用 FamilyPDF OCR 建立文字層。
- 已用繁／簡／英文 fixture 自動重讀；尚未在兩台不同中文語系 Windows
  上完成 Office GUI 人工巡覽。
