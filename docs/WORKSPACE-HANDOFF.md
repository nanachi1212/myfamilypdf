# FamilyPDF 工作狀態與接續說明

最後更新：2026-07-29（Asia/Taipei）

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
- 290 頁 PDF 開啟與保持回應驗證。
- Tesseract 5.5.2 OCR，包含繁中、簡中與英文語言資料。
- 繁中、簡中、英文安裝介面及 Qt 翻譯檔。
- 可攜式 ZIP 與目前使用者範圍的 Inno Setup 安裝檔。

## 最終產物

```text
E:\CodexProject\FamilyPDF\dist\FamilyPDF-windows-x64.zip
E:\CodexProject\FamilyPDF\dist\FamilyPDF-Setup-x64.exe
```

2026-07-29 驗證值：

| 檔案 | 大小 | SHA-256 |
|---|---:|---|
| `FamilyPDF-windows-x64.zip` | 96,063,098 bytes | `02B69D694F7C09B0369E4F8A70576B88791C4B78A647F030D3F116C950F59096` |
| `FamilyPDF-Setup-x64.exe` | 74,105,033 bytes | `CB0F2A396F27F63948009B607DDBD9628BA730D1844527795A1543213CCAC6DA` |

## 已通過驗證

- 可攜式包與靜默安裝後的三組單元測試：全部 exit code `0`。
- 安裝程式 `/VERYSILENT`：exit code `0`。
- Tesseract `--version`、`--list-langs` 與完整 OCR：exit code `0`。
- OCR 能辨識繁體中文、英文與 PDF 內的自由文字註解。
- 書籤跨 Editor 重啟驗證：JSON 計數 `1 → 0 → 1`。
- 安裝後 PdfTool：合併成 58 頁、拆分成 58 檔、290 頁資訊皆通過。
- 安裝後 Viewer 與 PageMaster 開啟 290 頁 PDF 8 秒後仍 Responding。

完整證據見 `docs\phase1\functional-verification.md`。

## 重建方式

PowerShell 執行政策受限時，使用完整 Windows PowerShell 路徑：

```powershell
cd E:\CodexProject\FamilyPDF
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-upstream-baseline.ps1 -Stage All
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\package-windows-runtime.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-installer.ps1 -SkipPackage
```

建置工具位於 `E:\CodexProject\FamilyPDF-tools`。OCR 語言資料與相依套件缺少時，封裝腳本會自動下載。

## 已知限制

- OCR 第一版輸出 UTF-8 純文字，不建立可搜尋 PDF 隱形文字層。
- 尚未在繁中與簡中兩套 Windows 實機進行完整人工巡覽。
- 安裝檔沒有商業程式碼簽章，SmartScreen 可能顯示未知發行者。
- `dist\` 不提交 Git；發布時需另外上傳 ZIP 與安裝檔。
