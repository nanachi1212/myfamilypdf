# Phase 0 基線與 Windows 工具鏈盤點

盤點日期：2026-07-27（Asia/Taipei）

本文件只記錄目前機器的唯讀盤點結果；本階段沒有安裝依賴，也沒有執行專案 build。

## Repository baseline

| 項目 | 實際結果 |
| --- | --- |
| Repository | `E:\CodexProject\FamilyPDF` |
| Branch | `codex/phase0-baseline` |
| Upstream commit | `53cac7cea9eb99f317366745a74d14be5c3df964` |
| Upstream commit date | `2026-07-12T21:04:32+02:00` |
| Remote URL | `https://github.com/JakubMelka/PDF4QT.git` |
| License | MIT License（根目錄 `LICENSE`；Copyright 2018-2025 Jakub Melka and Contributors） |
| Clone baseline cleanliness | `git status --short --branch` 只有 `## codex/phase0-baseline`，沒有 tracked 或 untracked 變更 |

基線查核命令與輸出：

```text
> git rev-parse HEAD
53cac7cea9eb99f317366745a74d14be5c3df964

> git show -s --format=%cI HEAD
2026-07-12T21:04:32+02:00

> git remote get-url origin
https://github.com/JakubMelka/PDF4QT.git

> git status --short --branch
## codex/phase0-baseline
```

## Available

### Git

```text
> Get-Command git
C:\Program Files\Git\cmd\git.exe

> git --version
git version 2.54.0.windows.1
```

### Visual Studio、MSBuild 與 C++ compiler

```text
> vswhere.exe -latest -products * -property installationPath
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools

> vswhere.exe -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe

> MSBuild.exe -version -nologo
18.6.3.22110

> vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools

> cl.exe
Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36246 for x64
```

`cl.exe` 實際路徑：

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe
```

注意：`cl.exe` 不在一般 PowerShell 的 `PATH`，必須透過 Visual Studio developer environment 或明確路徑使用。上游 README 寫明需要 C++20，並列出 Visual Studio 2022 或 MinGW；目前偵測到的是 Visual Studio Build Tools 2026，因此正式 build 前仍需做相容性驗證。

### Python

```text
> Get-Command python
C:\Users\User\AppData\Local\hermes\hermes-agent\venv\Scripts\python.exe

> python --version
Python 3.13.13
```

這是 Hermes 內含環境的 Python，不視為專案已建立獨立 Python toolchain。

## Missing

以下工具在目前 `PATH` 或列出的常見安裝位置找不到。本階段依要求沒有安裝。

### CMake

```text
> Get-Command cmake
MISSING
```

### Ninja

```text
> Get-Command ninja
MISSING
```

### Qt / qmake / qtpaths

```text
> Get-Command qmake,qmake6,qtpaths,qtpaths6
MISSING

> Test-Path C:\Qt
False

> Test-Path E:\Qt
False
```

上游 README 要求 Qt 6.9 或更新版本。

### vcpkg

```text
> Get-Command vcpkg
MISSING

> $env:VCPKG_ROOT
UNSET

> Test-Path C:\vcpkg,E:\vcpkg,C:\src\vcpkg
False
False
False
```

Repository 內有 `vcpkg.json` manifest 與 overlay 資料，但沒有可執行的 vcpkg installation。

### WiX Toolset

```text
> Get-Command wix,candle,light,heat
MISSING

> Test-Path "C:\Program Files (x86)\WiX Toolset v3.11"
False

> Test-Path "C:\Program Files\WiX Toolset v4"
False

> Test-Path "C:\Program Files\WiX Toolset v5"
False
```

上游 `WixInstaller\PDF4QT.wixproj.in` 使用 WiX v3.x targets；WiX 是建立 MSI 的阻擋項，但不是先完成一般 configure/build 的必要工具。

### Tesseract

```text
> Get-Command tesseract
MISSING
```

目前上游 `vcpkg.json` 沒有 Tesseract dependency，因此它不是原始 PDF4QT baseline build 的直接阻擋項；若 FamilyPDF Phase 0 要驗證 OCR，則必須另行決定支援版本與整合方式。

### 其他 compiler（PATH）

```text
> Get-Command clang-cl,clang,gcc,g++
MISSING
```

MSVC 已存在，所以這些替代 compiler 缺少不構成目前 baseline 的直接 blocker。

## Blockers

目前不能開始可重現的 Windows configure/build，直接阻擋項如下：

1. 缺少 `cmake`。
2. 缺少 Qt 6.9+（`qmake` / `qtpaths` 亦不可用）。
3. 缺少可執行的 vcpkg installation，且 `VCPKG_ROOT` 未設定。
4. 上游 Windows sample 使用 Ninja，但目前缺少 `ninja`。若改採 Visual Studio generator，可在下一階段明確決策後不把 Ninja 列為硬性需求。

MSI packaging 另受缺少 WiX Toolset 阻擋。OCR 驗證另受缺少 Tesseract 阻擋。Visual Studio Build Tools 2026、MSBuild 與 MSVC compiler 已存在，不是立即阻擋項，但相較上游明列的 Visual Studio 2022 尚未經 build 驗證。

## 下一步

1. 先決定可重現的 Windows toolchain 組合：Visual Studio generator，或 Ninja + MSVC。
2. 依專案核准版本安裝 CMake、Qt 6.9+ 與 vcpkg；若採 Ninja generator，再安裝 Ninja。
3. 明確設定 `VCPKG_ROOT`、`CMAKE_TOOLCHAIN_FILE`、`CMAKE_PREFIX_PATH` / `PDF4QT_QT_ROOT`。
4. 僅在 MSI 進入交付範圍後補 WiX v3 相容 toolchain；僅在 OCR 進入 Phase 0 驗證範圍後補 Tesseract。
5. 工具到齊後再執行 configure；首次 configure/build 的命令、完整版本與結果應另寫 Phase 0 build evidence，不回寫成本文件中的既有基線。
