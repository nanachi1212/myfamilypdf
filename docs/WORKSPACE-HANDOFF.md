# FamilyPDF 工作交接說明

最後更新：2026-07-28（Asia/Taipei）

## 目前狀態

- GitHub：`https://github.com/nanachi1212/myfamilypdf`
- 分支：`codex/phase0-baseline`
- 目前 commit：待本次 log 修正提交後更新
- 官方來源 remote：`upstream = https://github.com/JakubMelka/PDF4QT.git`
- 本專案 remote：`origin = https://github.com/nanachi1212/myfamilypdf.git`
- 工作樹在最後檢查時乾淨。
- OneDrive 工作副本：`E:\OneDrive\myfamilypdf`

## 正在進行的工作

原版 PDF4QT Windows x64 Release baseline 的前一次 configure 在 vcpkg `openssl` 依賴階段中斷；已完成的 vcpkg binary cache 會重用，下一步從 Configure 重跑。尚未完成 configure、主程式 build 或 upstream tests，因此不可把 Phase 0 baseline 說成已通過。

建置輸出位於工作機：

```text
E:\CodexProject\FamilyPDF\build\phase0-upstream-release
```

主要 log：`configure.log`、`vcpkg-manifest-install.log`。這些 build artifacts 不提交到 Git。

## 下次接續順序

在 PowerShell：

```powershell
cd E:\OneDrive\myfamilypdf
git pull --ff-only origin codex/phase0-baseline
git status -sb
```

若要在原工作機繼續建置：

```powershell
& 'C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File '.\scripts\phase0\build-upstream-baseline.ps1' -Stage Configure
& 'C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File '.\scripts\phase0\build-upstream-baseline.ps1' -Stage Build
& 'C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File '.\scripts\phase0\build-upstream-baseline.ps1' -Stage Test
```

先檢查是否仍有舊的 `cmake`/`vcpkg` process；若 configure 已完成，直接從 `Build` 開始。完成後要記錄 targets、CTest 結果、耗時、輸出檔案位置與失敗原因，再做規格審查及品質審查。

## 已完成的 Phase 0 工具鏈

- Qt 6.9.1 `win64_msvc2022_64`
- aqtinstall 3.3.0 local virtual environment
- vcpkg commit `6d9d7df564a1ccdaa994e4ad39ccd4a32360867b`
- vcpkg tool release `2026-07-13-bf04c909169fdbb30821c02c6eb01f1cd1295d05`
- Visual Studio bundled CMake 4.2.3-msvc3、Ninja 1.13.2
- 工具隔離目錄：`E:\CodexProject\FamilyPDF-tools`

安裝紀錄：`docs/phase0/toolchain-install.md`。工具安裝腳本：`scripts/phase0/install-build-toolchain.ps1`。

## 注意事項

目前 build script 將 Qt/vcpkg 路徑固定為 `E:\CodexProject\FamilyPDF-tools`，所以 OneDrive 副本目前主要用途是保存程式碼、Git 歷史和進度；要在家裡編譯，下一個便利性任務應先把 `ToolsRoot`、Qt prefix、CMake/Ninja 路徑改成可參數化，並重新驗證。

尚未安裝 WiX Toolset 或 Tesseract；它們應分別延後到 MSI 與 OCR 任務。進入 OCR 不可見文字層座標、PDF 保存／當機恢復等高推理架構決策前，請切換到 Sol。

整體產品規劃仍以工作區的 `pdf-reader-plan.md` 為準；Phase 0A 未完成前，不要承諾 OCR 或完整家庭版工期。
