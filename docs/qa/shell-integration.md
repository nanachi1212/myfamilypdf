# Windows PDF 整合安裝／解除安裝驗證

更新日期：2026-07-30

## 驗證範圍

`scripts\qa\smoke-shell-installation.ps1` 使用獨立 AppId 建立 Shell 驗證
安裝器，並安裝到 `build\shell-installer-smoke\app`。執行前會先確認測試
使用的 Registry 鍵不存在；若偵測到既有鍵便停止，不會覆寫現有設定。

驗證內容：

- 安裝程式以 `/TASKS=pdfshell` 靜默安裝，exit code 必須為 `0`。
- Windows「開啟方式」的 Viewer／Editor 命令均為完整引用的
  `"<exe>" "%1"`。
- PDF 右鍵「使用 FamilyPDF 開啟／編輯」命令均為完整引用的
  `"<exe>" "%1"`。
- Viewer 與 Editor 實際開啟含中文及空格的 PDF 路徑，五秒後仍須保持
  Responding。
- 解除安裝 exit code 必須為 `0`。
- FamilyPDF Shell Registry 子樹、測試解除安裝項目及安裝檔案均須移除。

## 執行

```powershell
.\scripts\qa\smoke-shell-installation.ps1
```

若驗證安裝器已建好，可略過重建：

```powershell
.\scripts\qa\smoke-shell-installation.ps1 -SkipBuild
```

結果寫入 `build\shell-installer-smoke-summary.json`。2026-07-30 的實測結果
為安裝及解除安裝 exit code `0`，Viewer／Editor 均保持 Responding，且
Registry 與安裝檔案均已完整移除。
