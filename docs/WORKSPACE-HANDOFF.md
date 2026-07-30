# FamilyPDF 工作狀態與接續說明

最後更新：2026-07-30（Asia/Taipei）

## 專案位置

- 主要工作樹：`E:\CodexProject\FamilyPDF`
- GitHub：`https://github.com/nanachi1212/myfamilypdf`
- 分支：`codex/phase0-baseline`
- OneDrive 副本：`E:\OneDrive\myfamilypdf`
- 上游：`https://github.com/JakubMelka/PDF4QT.git`

## 目前完成狀態

FamilyPDF Windows x64 家庭版已具備可安裝、可攜式與可驗證的完整第一版：

- Viewer、Editor、PageMaster、PdfTool。
- 彩色文字標記、框選、自由文字、註解與側欄。
- Viewer／Editor 共用且跨重啟保存的本機書籤。
- PDF 合併、拆分、單數頁、雙數頁與自訂頁碼範圍。
- 1,160 頁 PDF 在繁中／簡中 GUI 完成頁碼與縮略圖跳轉並保持回應。
- Tesseract 5.5.2 OCR，包含繁中、簡中與英文語言資料。
- 繁中、簡中、英文安裝介面及 Qt 翻譯檔。
- 可攜式 ZIP、基礎安裝檔、預設包含 OCR 的一鍵完整安裝檔，以及獨立 OCR 外掛安裝檔。
- 五種標準 AcroForm 欄位建立、文件級浮水印／背景／頁面幾何編輯。
- 內建 Office Export 外掛，可將可搜尋 PDF 匯出為 DOCX／XLSX，使用端不需 Python。

## 最終產物

```text
E:\CodexProject\FamilyPDF\dist\FamilyPDF-windows-x64.zip
E:\CodexProject\FamilyPDF\dist\FamilyPDF-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-Full-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-OCR-Plugin-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-OCR-Plugin-windows-x64.zip
```

2026-07-30 驗證值：

| 檔案 | 大小 | SHA-256 |
|---|---:|---|
| `FamilyPDF-Full-Setup-x64.exe` | 69,623,370 bytes | `E45762B74D20EEC72606907309A6AFC7FDF747345557604ABDE26224027289C5` |
| `FamilyPDF-Setup-x64.exe` | 58,732,866 bytes | `39E3E9C74BE307F0680602F5C012A70D5ACACF3868F7D42192082FB9449AEA71` |
| `FamilyPDF-windows-x64.zip` | 83,863,232 bytes | `54526F899CEDE47D21625351B42B5345DA0A6F6BCC371C4B943FBFBC66BE7D1A` |
| `FamilyPDF-OCR-Plugin-Setup-x64.exe` | 14,049,516 bytes | `C40944A35AEE045DA4C1DC339AD62FB1C63F6F507E562D2EBFECA5773903C801` |
| `FamilyPDF-OCR-Plugin-windows-x64.zip` | 14,879,477 bytes | `C855BB0911C6CFD376A2F11CDCBE5885AACB942A2584272194F6ECE930029BD4` |

## 已通過驗證

- CTest 6/6 與 Office Export Python 單元測試 7/7 通過。
- 基礎、完整與精簡安裝流程 exit code `0`；完整模式包含 OCR，精簡模式不含 OCR。
- Tesseract `--version`、`--list-langs` 與完整 OCR：exit code `0`。
- OCR 能辨識繁體中文、簡體中文與英文，並輸出保留頁數的可搜尋 PDF 及 UTF-8 文字。
- 書籤跨 Editor 重啟驗證：JSON 計數 `1 → 0 → 1`。
- 安裝後 PdfTool：合併成 58 頁，單數／雙數／`10-20` 範圍為 29／29／11 頁，1,160 頁資訊通過。
- Viewer／Editor 同時載入三份 PDF；1,160 頁檔案在繁中／簡中 GUI 完成頁碼與縮略圖跳轉並保持 Responding。
- Adobe Acrobat DC 實際修改並另存 AcroForm 後，中文欄位名稱、值與預設值由 `pypdf` 獨立讀回。
- Microsoft Word／Excel 16.0 實際開啟匯出產物，頁數、工作表、多語文字、表格值與合併儲存格通過。

完整證據見 `docs\RELEASE-STATUS.md`、`docs\REQUIREMENTS-AUDIT.md` 與 `docs\qa\release-checklist.md`。

## 重建方式

PowerShell 執行政策受限時，使用完整 Windows PowerShell 路徑：

```powershell
cd E:\CodexProject\FamilyPDF
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-upstream-baseline.ps1 -Stage All
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\package-windows-runtime.ps1 -SkipOcr
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage
```

建置工具位於 `E:\CodexProject\FamilyPDF-tools`。OCR 語言資料與相依套件缺少時，封裝腳本會自動下載。

## 已知限制

- 尚未在繁中與簡中兩套 Windows 實機進行完整人工巡覽。
- 文件級編輯與複雜 Office 排版仍需真人視覺巡覽；自動測試只驗證 PDF／Office 結構與關鍵內容。
- 安裝檔沒有商業程式碼簽章，SmartScreen 可能顯示未知發行者。
- `dist\` 不提交 Git；發布時需另外上傳 ZIP 與安裝檔。
