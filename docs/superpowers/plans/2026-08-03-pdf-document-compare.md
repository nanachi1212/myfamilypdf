# PDF 文件比較交付實作計畫

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 將既有 PDF4QT `Pdf4QtDiff` 正式交付為 FamilyPDF 文件比較工具，支援同時載入兩份 PDF、自動比較文字／圖片／向量差異、繁簡中文介面、差異巡覽與比較報告。

**Architecture:** 不重寫比較引擎，直接把成熟的 `Pdf4QtDiff` target 接入 FamilyPDF 的固定工具鏈、可攜包與兩種安裝器。新增一個 package-level QA，使用 `pypdf` 建立只差一個文字字元的真實 PDF，先由 `PdfTool diff` 獨立確認比較引擎輸出，再以兩個檔案啟動 `Pdf4QtDiff` 並確認 GUI 存活、可回應及載入後未退出；完整回歸與安裝 smoke 共同證明可攜版、完整安裝與精簡安裝都包含相同執行檔。

**Tech Stack:** C++20、Qt 6.9.1、PDF4QT Diff／PdfTool、PowerShell QA、Python 3.14 + pypdf fixture、Inno Setup 7.0.2。

---

### Task 1: 建立文件比較交付回歸測試

**Files:**
- Create: `scripts/qa/smoke-pdf-diff.ps1`
- Modify: `scripts/qa/run-final-regression.ps1`

- [x] **Step 1: 建立只差一個文字字元的真實 PDF fixture**

在 `smoke-pdf-diff.ps1` 接受 `PackageDirectory` 與可選 `OutputDirectory`，使用已鎖定的 Office Python 建立兩份一頁 PDF：

```powershell
$python = Join-Path (Split-Path $repositoryRoot -Parent) `
    'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
& $python -c @"
from pathlib import Path
from pypdf import PdfWriter
from pypdf.generic import DecodedStreamObject, DictionaryObject, NameObject

def write_pdf(path: Path, text: str) -> None:
    writer = PdfWriter()
    page = writer.add_blank_page(width=300, height=300)
    font = DictionaryObject({
        NameObject('/Type'): NameObject('/Font'),
        NameObject('/Subtype'): NameObject('/Type1'),
        NameObject('/BaseFont'): NameObject('/Helvetica'),
    })
    page[NameObject('/Resources')] = DictionaryObject({
        NameObject('/Font'): DictionaryObject({
            NameObject('/F1'): writer._add_object(font),
        }),
    })
    stream = DecodedStreamObject()
    stream.set_data(f'BT /F1 20 Tf 40 200 Td ({text}) Tj ET'.encode('ascii'))
    page[NameObject('/Contents')] = writer._add_object(stream)
    with path.open('wb') as handle:
        writer.write(handle)

root = Path(r'$OutputDirectory')
write_pdf(root / 'left.pdf', 'Version A')
write_pdf(root / 'right.pdf', 'Version B')
"@
```

- [x] **Step 2: 以命令列引擎驗證可觀測差異**

執行 package 內的 `PdfTool.exe`，要求 XML 含一筆 `text-replaced`，且 added／removed 分別為 `B`／`A`：

```powershell
$xmlText = (& $pdfTool diff --console-format xml $leftPdf $rightPdf 2>&1 |
    Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "PdfTool diff failed.`n$xmlText"
}
[xml]$xml = $xmlText
$difference = $xml.'difference-report'.differences.difference
if ($difference.type -ne 'text-replaced' -or
    $difference.'text-added' -ne 'B' -or
    $difference.'text-removed' -ne 'A') {
    throw "Unexpected diff XML.`n$xmlText"
}
```

- [x] **Step 3: 以兩份 PDF 啟動 GUI 並驗證存活**

使用隔離設定資料夾啟動 `Pdf4QtDiff.exe`；最多等待 45 秒取得視窗，確認 process 沒退出且 `Responding`，最後保證關閉測試 process：

```powershell
$process = Start-Process -FilePath $diffGui -ArgumentList @(
    '-c', $settingsDirectory, $leftPdf, $rightPdf
) -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(45)
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
    } while (-not $process.HasExited -and
        $process.MainWindowHandle -eq 0 -and
        [DateTime]::UtcNow -lt $deadline)
    if ($process.HasExited -or $process.MainWindowHandle -eq 0 -or
        -not $process.Responding) {
        throw 'Pdf4QtDiff did not remain responsive with two input PDFs.'
    }
}
finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
```

寫出 `summary.json`，記錄差異類型、GUI Responding、working set 與兩份 fixture。

- [x] **Step 4: 執行測試並確認 RED**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-pdf-diff.ps1 -PackageDirectory dist\FamilyPDF-windows-x64
```

Expected: FAIL，指出 `Pdf4QtDiff.exe` 不存在；`PdfTool.exe` 已存在。

- [x] **Step 5: 將 smoke 接入完整回歸**

在 `run-final-regression.ps1` 的必要檔案清單加入 `Pdf4QtDiff.exe`，複製可攜包後執行：

```powershell
$diffSmoke = & (Join-Path $PSScriptRoot 'smoke-pdf-diff.ps1') `
    -PackageDirectory $qaPackage `
    -OutputDirectory (Join-Path $qaRoot 'pdf-diff')
```

在最終 `summary.json` 加入：

```powershell
document_compare = [ordered]@{
    gui_responding = $true
    cli_difference_type = 'text-replaced'
}
```

### Task 2: 接入建置、可攜包與安裝器

**Files:**
- Modify: `scripts/phase0/build-upstream-baseline.ps1`
- Modify: `scripts/phase0/package-windows-runtime.ps1`
- Modify: `scripts/phase0/build-full-installer.ps1`
- Modify: `installer/FamilyPDF.iss`
- Modify: `installer/FamilyPDF-Full.iss`
- Modify: `scripts/qa/smoke-full-installer.ps1`

- [x] **Step 1: 將 `Pdf4QtDiff` 加入固定 build target**

在 `$Targets` 的三個 GUI target 後加入：

```powershell
    'Pdf4QtDiff',
```

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-upstream-baseline.ps1 -Stage Build
```

Expected: `build\phase0-upstream-release\usr\bin\Pdf4QtDiff.exe` 存在，build exit code `0`。

- [x] **Step 2: 將比較工具加入正式 runtime 部署**

在 `package-windows-runtime.ps1` 的 `$targets` 加入 `Pdf4QtDiff`，讓同一次 `windeployqt` 分析其依賴；在完整安裝器輸入檢查加入：

```powershell
    (Join-Path $basePackage 'Pdf4QtDiff.exe'),
```

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1 -SkipOcr
```

Expected: 可攜包與 ZIP 皆重建，無 `VCINSTALLDIR`、DXC 或 Qt fallback 警告。

- [x] **Step 3: 為繁中、簡中與英文安裝器加入捷徑名稱**

在兩個 `.iss` 的 `[CustomMessages]` 加入：

```ini
chinesetraditional.CompareShortcut=FamilyPDF 文件比較
chinesesimplified.CompareShortcut=FamilyPDF 文档比较
english.CompareShortcut=FamilyPDF Document Compare
```

在 `[Icons]` 加入：

```ini
Name: "{group}\{cm:CompareShortcut}"; Filename: "{app}\Pdf4QtDiff.exe"; WorkingDir: "{app}"
```

- [x] **Step 4: 擴充完整與精簡安裝檔案驗證**

在 `smoke-full-installer.ps1` 的完整模式與 core 模式 required file list 都加入：

```powershell
    'Pdf4QtDiff.exe',
```

並在完整安裝根目錄執行 `smoke-pdf-diff.ps1`，確認安裝版 GUI 與 CLI 比較均通過。

- [x] **Step 5: 重建核心與完整安裝器並確認 GREEN**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1
```

Expected: 完整與精簡安裝 exit code `0`，兩者都有 `Pdf4QtDiff.exe`，安裝版比較 smoke 通過。

### Task 3: 完整回歸、文件與交付

**Files:**
- Modify: `README.md`
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 執行來源與封裝測試**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-pdf-diff.ps1 -PackageDirectory dist\FamilyPDF-windows-x64
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\test-windeployqt-environment.ps1
```

Expected: 比較 XML、GUI、Qt runtime 皆通過。

- [x] **Step 2: 執行最終完整回歸**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1
```

Expected: CTest 6/6、Office 9/9、OCR、1,160 頁、多文件、PDF 比較 GUI／CLI 全部通過，且只保留最新回歸結果。

- [x] **Step 3: 更新家庭功能與使用說明**

文件明確加入：

- `Pdf4QtDiff.exe` 可選兩份 PDF，比對文字、圖片、向量、著色與頁面移動。
- 可切換合併、左側、右側與疊加檢視，巡覽上一個／下一個差異。
- 可輸出標示差異的 PDF report 或 XML。
- 自動 fixture 證明 `Version A` → `Version B` 為 `text-replaced`；不宣告已涵蓋所有複雜透明度與 DRM 文件。

- [x] **Step 4: 更新正式產物 bytes、SHA-256 與回歸路徑**

以 `Get-FileHash -Algorithm SHA256` 重算三個核心產物；OCR 外掛未改時保留既有雜湊。確認文件中的 bytes 與實際 `dist` 完全一致。

- [x] **Step 5: 提交並嘗試推送**

```powershell
git add README.md docs installer scripts
git commit -m "feat: deliver PDF document comparison"
git push origin codex/phase0-baseline
```

若 `github.com:443` 或無效 token 仍阻擋推送，保留乾淨且領先遠端的本機提交，並只回報一次具體登入／網路條件。
