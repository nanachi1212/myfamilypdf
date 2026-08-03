# FamilyPDF 工作狀態與接續說明

最後更新：2026-08-03（Asia/Taipei）

## 專案位置

- 主要工作樹：`E:\CodexProject\FamilyPDF`
- GitHub：`https://github.com/nanachi1212/myfamilypdf`
- 分支：`codex/phase0-baseline`
- OneDrive 副本：`E:\OneDrive\myfamilypdf`
- 上游：`https://github.com/JakubMelka/PDF4QT.git`

## 目前完成狀態

FamilyPDF Windows x64 家庭版已具備可安裝、可攜式與可驗證的完整第一版：

- Viewer、Editor、PageMaster、PdfTool、Pdf4QtDiff 文件比較工具。
- 彩色文字標記、框選、自由文字、註解與側欄。
- Viewer／Editor 共用且跨重啟保存的本機書籤。
- PDF 合併、拆分、單數頁、雙數頁與自訂頁碼範圍。
- 1,160 頁 PDF 在繁中／簡中 GUI 完成頁碼與縮略圖跳轉並保持回應。
- Tesseract 5.5.2 OCR，包含繁中、簡中與英文語言資料。
- 繁中、簡中、英文安裝介面及 Qt 翻譯檔。
- 可攜式 ZIP、基礎安裝檔、預設包含 OCR 的一鍵完整安裝檔，以及獨立 OCR 外掛安裝檔。
- 五種標準 AcroForm 欄位建立、文件級浮水印／背景／頁面幾何編輯。
- 內建 Office Export 外掛，可將可搜尋 PDF 匯出為 DOCX／XLSX，使用端不需 Python。
- 文件比較可自動比對兩份 PDF 的文字、圖片、向量、著色與頁面移動，支援差異巡覽、左右／疊加檢視與 PDF／XML report。

## 最終產物

```text
E:\CodexProject\FamilyPDF\dist\FamilyPDF-windows-x64.zip
E:\CodexProject\FamilyPDF\dist\FamilyPDF-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-Full-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-OCR-Plugin-Setup-x64.exe
E:\CodexProject\FamilyPDF\dist\FamilyPDF-OCR-Plugin-windows-x64.zip
```

2026-08-03 驗證值：

| 檔案 | 大小 | SHA-256 |
|---|---:|---|
| `FamilyPDF-Full-Setup-x64.exe` | 88,089,244 bytes | `C05D6338E5CA14398445E1EB00CA6E355515CAC30CAEA7A6CE92851B027EBBE7` |
| `FamilyPDF-Setup-x64.exe` | 77,198,447 bytes | `E1CD7694E960382DF34F1C6E59962F47E79502C3B7E70264C71728DA2D836362` |
| `FamilyPDF-windows-x64.zip` | 101,477,931 bytes | `E4DA8A0CB4172C47E3F93A1BE026FB104D06C8861AE113FB1EB334D2764575DA` |
| `FamilyPDF-OCR-Plugin-Setup-x64.exe` | 14,049,516 bytes | `C40944A35AEE045DA4C1DC339AD62FB1C63F6F507E562D2EBFECA5773903C801` |
| `FamilyPDF-OCR-Plugin-windows-x64.zip` | 14,879,477 bytes | `C855BB0911C6CFD376A2F11CDCBE5885AACB942A2584272194F6ECE930029BD4` |

## 已通過驗證

- CTest 6/6 與 Office Export Python 單元測試 9/9 通過。
- 基礎、完整與精簡安裝流程 exit code `0`；完整模式包含 OCR，精簡模式不含 OCR。
- Tesseract `--version`、`--list-langs` 與完整 OCR：exit code `0`。
- OCR 能辨識繁體中文、簡體中文與英文，並輸出保留頁數的可搜尋 PDF 及 UTF-8 文字。
- 書籤跨 Editor 重啟驗證：JSON 計數 `1 → 0 → 1`。
- 安裝後 PdfTool：合併成 58 頁，單數／雙數／`10-20` 範圍為 29／29／11 頁，1,160 頁資訊通過。
- Viewer／Editor 同時載入三份 PDF；1,160 頁檔案在繁中／簡中 GUI 完成頁碼與縮略圖跳轉並保持 Responding。
- `Pdf4QtDiff.exe` 在可攜版與完整安裝版均以兩份真實 PDF 啟動並保持 Responding；命令列引擎確認 `Version A` → `Version B` 為 `text-replaced`，移除 `A`、加入 `B`。
- Adobe Acrobat DC 實際修改並另存 AcroForm 後，中文欄位名稱、值與預設值由 `pypdf` 獨立讀回。
- Adobe Acrobat DC 實際逐頁解析並另存浮水印／背景與頁面幾何 fixture，獨立 parser 結構及逐頁 RGB 渲染雜湊均保留。
- 文件級編輯 fixture 已加入頁碼、方向箭頭、外框與四色角標；2026-08-03 的 Poppler 五頁巡覽確認背景、繁中浮水印、裁切與 90 度旋轉均正確。
- Microsoft Word／Excel 16.0 實際開啟三頁複合匯出產物，十個段落、五種字級、分頁、粗斜體、三工作表、同頁多表格、四行 fallback、UsedRange、多語文字、跨欄合併、各表表頭樣式與欄寬通過；Word 原生 renderer 另產生三頁 PNG，固定 fixture 巡覽未見亂碼、缺字方框、裁切或重疊。
- 完整／精簡安裝後的核心與 OCR payload 已逐檔比對目前可攜包 SHA-256。

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
- 文件級編輯固定 fixture 已完成真人巡覽，仍需以任意實際文件判斷浮水印透明度、背景圖片裁切與複雜內容品質；Office writer 支援範圍內的三頁複合 fixture 已完成 Office 本體解析、Word 原生渲染與巡覽。一般等寬雙欄、全寬文字與非重疊 raster 圖片已完成真實 PDF、封裝 helper 與 OOXML 重讀；向量圖形、透明遮罩精確重建、文字環繞／重疊浮動物件、不等寬／三欄以上、跨頁表格及精確座標版面目前未重建。
- 安裝檔沒有商業程式碼簽章，SmartScreen 可能顯示未知發行者。
- 文件比較固定 fixture 尚未涵蓋複雜透明混合、DRM／密碼文件或大型工程圖，這些文件仍需人工判讀結果。

## 2026-08-03 接續狀態

- Office Export Python 環境已具備自動健康檢查與修復能力；目前使用本機 vcpkg Python 3.14.2，無須使用者手動安裝 Python。
- 最新完整回歸：`build\final-regression-20260803-135541\summary.json`，CTest 6/6、Office Python 9/9、封裝 helper raster 圖片／跨欄多區段／雙欄／單欄相容回歸、PDF 文件比較 CLI／GUI、OCR 繁簡中文與 1,160 頁多文件測試均通過；Qt runtime 沒有環境警告或 fallback。
- `scripts\qa\cleanup-final-regression-results.ps1` 會在成功回歸後自動只保留最新結果；舊的安裝展開副本已清除，正式釋出產物與最新版安裝驗證保留。
- `dist\` 不提交 Git；發布時需另外上傳 ZIP 與安裝檔。
