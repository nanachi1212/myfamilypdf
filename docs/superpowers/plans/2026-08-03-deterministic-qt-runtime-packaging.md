# Deterministic Qt Runtime Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 Qt 6.9.1 `windeployqt` 偶發 `0xC0000409`，讓 FamilyPDF 可攜包每次都使用固定、可驗證且無環境警告的 release runtime。

**Architecture:** 封裝腳本不再啟動 `windeployqt`，而是直接複製已由 PE imports、CTest 與 GUI 冒煙測試驗證的 10 個 Qt modules、15 個 plugins，以及 DXC、VC runtime。QA 會把「不得呼叫 windeployqt」列為明確建置不變量，並實際檢查完整 runtime 與 GUI。

**Tech Stack:** PowerShell 5.1、Qt 6.9.1、PE runtime、Inno Setup、CTest。

---

## File structure

- Modify: `scripts/phase0/package-windows-runtime.ps1` — 移除不穩定工具呼叫，固定部署 release runtime。
- Modify: `scripts/qa/test-windeployqt-environment.ps1` — 驗證腳本不再執行 windeployqt、沒有警告且 runtime 完整。
- Modify: `docs/REQUIREMENTS-AUDIT.md`、`docs/RELEASE-STATUS.md`、`docs/WORKSPACE-HANDOFF.md` — 記錄確定性封裝證據與新產物雜湊。

### Task 1: 建立不得呼叫 windeployqt 的 RED 契約

**Files:**
- Modify: `scripts/qa/test-windeployqt-environment.ps1`

- [x] **Step 1: 新增腳本執行不變量**

讀取 `package-windows-runtime.ps1` 原文，若符合 `& $windeployqt` 呼叫則失敗：

```powershell
$packageScriptText = Get-Content -LiteralPath $packageScript -Raw
if ($packageScriptText -match '(?im)^\s*&\s+\$windeployqt\b') {
    throw 'Packaging must not execute the unstable Qt 6.9.1 windeployqt.'
}
```

並將 `windeployqt failed with exit code` 加入禁止的環境警告字串。

- [x] **Step 2: 執行測試並確認 RED**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\test-windeployqt-environment.ps1
```

Expected: FAIL，指出封裝腳本仍執行不穩定的 `windeployqt`。

### Task 2: 改為固定 release runtime 部署

**Files:**
- Modify: `scripts/phase0/package-windows-runtime.ps1`
- Test: `scripts/qa/test-windeployqt-environment.ps1`

- [x] **Step 1: 移除 windeployqt 前置條件與呼叫**

移除 `$windeployqt`、`VCINSTALLDIR`／`VSINSTALLDIR` 暫存與整段外部部署呼叫；保留 Qt prefix、runtime directory、DirectX 與 VC runtime 檢查。

- [x] **Step 2: 無條件複製固定 Qt runtime**

把原 fallback 內的 `Qt6Concurrent.dll` 至 `Qt6Xml.dll` 十個 modules，以及 `platforms\qwindows.dll` 至 `tls\qschannelbackend.dll` 十五個 plugins 改為每次無條件逐檔存在檢查與複製。註解清楚說明清單由實際 PE imports 與 GUI 回歸維護。

- [x] **Step 3: 執行環境契約並確認 GREEN**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\test-windeployqt-environment.ps1
```

Expected: PASS；沒有 `windeployqt`、DXC、VCINSTALLDIR 或 fallback 警告，必要 runtime 全部存在。

- [x] **Step 4: 執行可攜版 GUI 與 CTest 回歸**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1 -SkipOfficeBuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1
```

Expected: CTest 6/6、Office 10/10、OCR、Viewer／Editor、多文件、PDF 比較全部通過，封裝輸出不含 Qt runtime fallback。

### Task 3: 重建正式產物與提交

**Files:**
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 重建兩種安裝檔**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1
```

Expected: 核心與完整安裝檔編譯成功且封裝階段沒有 windeployqt warning。

- [x] **Step 2: 執行完整／精簡隔離安裝**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1
```

Expected: 完整與精簡安裝 exit code `0`，406 個核心檔案逐檔相同；完整模式另驗證 29 個 OCR 檔案。

- [x] **Step 3: 更新文件與三個正式產物 SHA-256**

記錄最新完整／核心安裝檔與可攜 ZIP 的 bytes、SHA-256、最終回歸摘要路徑；將 Qt runtime 說明改成「固定部署，完全不執行 windeployqt」。

- [x] **Step 4: 提交並嘗試推送**

```powershell
git add scripts docs
git commit -m "fix: make Qt runtime packaging deterministic"
git push origin codex/phase0-baseline
```

Expected: 本機提交成功；若 `github.com:443` 仍不可達，乾淨分支安全保留並只回報外部阻礙。
