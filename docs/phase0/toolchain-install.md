# Phase 0A baseline build toolchain 安裝紀錄

記錄日期：2026-07-27（Asia/Taipei）

## 結果

Phase 0 baseline configure/build 立即需要的 Qt 與 vcpkg 已安裝在 `E:\CodexProject\FamilyPDF-tools`。本次沒有執行 PDF4QT configure 或 build，也沒有安裝 WiX Toolset 或 Tesseract。

| 工具 | 固定版本或基線 | 實際路徑 | 實際大小 |
| --- | --- | --- | ---: |
| Qt | 6.9.1，`win64_msvc2022_64` | `E:\CodexProject\FamilyPDF-tools\qt` | 2,560,774,752 bytes（約 2.38 GiB） |
| aqtinstall | 3.3.0，local venv | `E:\CodexProject\FamilyPDF-tools\aqt-3.3.0` | 28,252,860 bytes（約 26.94 MiB） |
| vcpkg source | commit `6d9d7df564a1ccdaa994e4ad39ccd4a32360867b` | `E:\CodexProject\FamilyPDF-tools\vcpkg` | 152,203,257 bytes（約 145.15 MiB） |
| vcpkg executable | `2026-07-13-bf04c909169fdbb30821c02c6eb01f1cd1295d05` | `E:\CodexProject\FamilyPDF-tools\vcpkg\vcpkg.exe` | 包含於上列 vcpkg 大小 |

安裝沒有修改全域 `PATH`、Registry 或 system Python。aqtinstall 使用自己的 virtual environment。

## 版本與 modules 的依據

版本及 modules 是從此 checkout 的 build 定義讀出，不是猜測：

- `README.md` 要求 Qt 6.9 或更高。
- `Dockerfile` 固定 Qt 6.9.1，並安裝 `qtmultimedia`、`qtspeech`。
- `.github/workflows/ci.yml` 的 Windows job 使用 `win64_msvc2022_64`，並安裝 `qtspeech`、`qtmultimedia`。
- 根 `CMakeLists.txt` 要求 Qt `Core`、`LinguistTools`、`Gui`、`Widgets`、`Svg`、`Xml`、`PrintSupport`、`TextToSpeech`、`Concurrent` 與測試預設啟用時的 `Test`。
- `qtimageformats` 只出現在 MSI workflow；本 task 不處理 MSI，因此沒有額外安裝。

Qt 6.9.1 是 repository 已明確使用、與 Windows CI 的 Qt 6.9.0 同一 minor line 的 patch。安裝後已逐一確認上述 Qt CMake package files 存在。

根 `vcpkg.json` 沒有 `builtin-baseline`，CI 也只 clone 當時的 vcpkg master。因此安裝腳本自行固定 2026-07-27 從官方 master 解析到的 exact commit `6d9d7df564a1ccdaa994e4ad39ccd4a32360867b`，避免日後重跑落到不同 port baseline。這個 pin 不會修改 upstream manifest。

## 固定來源

- Qt official release archives：`https://download.qt.io/official_releases/qt/6.9/6.9.1/`，由 aqtinstall 解析官方 mirror metadata 後下載。
- aqtinstall 3.3.0：PyPI official index `https://pypi.org/simple`。
- vcpkg：Microsoft official repository `https://github.com/microsoft/vcpkg.git`，checkout 到上述 exact commit。
- vcpkg bootstrap：該 commit 的 `bootstrap-vcpkg.bat -disableMetrics` 透過 TLS 從 Microsoft 的 GitHub release 下載對應 executable。upstream metadata 沒有提供 Windows binary hash，本腳本因此不宣稱獨立的 hash 或 Authenticode 驗證；它會比對 pinned metadata 的 release tag 與 executable 回報版本。

## 安裝與重跑

從 `E:\CodexProject\FamilyPDF` 執行：

```powershell
$env:PATH = 'E:\rtk-develop;' + $env:PATH
& 'C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe' `
  -NoProfile `
  -ExecutionPolicy Bypass `
  -File '.\scripts\phase0\install-build-toolchain.ps1'
```

若目前 `python.exe` 不是 Python 3.9 或更新版本，可明確指定只用來建立 local aqt venv 的 Python：

```powershell
.\scripts\phase0\install-build-toolchain.ps1 `
  -BootstrapPython 'C:\path\to\python.exe'
```

腳本具冪等性：

- local venv、正確版本 aqtinstall、完整且版本正確的 Qt、正確 vcpkg commit 與已 bootstrap 的 `vcpkg.exe` 都會重用。
- 若日後更新 pinned vcpkg commit，腳本會重新 bootstrap，並以該 commit 的 `VCPKG_TOOL_RELEASE_TAG` 驗證 executable，避免沿用舊 binary。
- 只有缺少或版本不符時才下載。
- 若目標路徑存在但不是可辨識的完整安裝，腳本會明確失敗，不會覆寫未知內容。
- 任一 native command 或下載失敗都會以 non-zero exit 結束。

## 驗證紀錄

安裝腳本已驗證：

```text
qmake.exe -query QT_VERSION
6.9.1

qtpaths.exe --qt-version
6.9.1

Qt6Config.cmake
E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake

vcpkg.exe version
vcpkg package management program version 2026-07-13-bf04c909169fdbb30821c02c6eb01f1cd1295d05

git rev-parse HEAD
6d9d7df564a1ccdaa994e4ad39ccd4a32360867b
```

實際 executable：

```text
E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64\bin\qmake.exe
E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64\bin\qtpaths.exe
E:\CodexProject\FamilyPDF-tools\vcpkg\vcpkg.exe
```

Qt CMake prefix：

```text
E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64
```

vcpkg toolchain file：

```text
E:\CodexProject\FamilyPDF-tools\vcpkg\scripts\buildsystems\vcpkg.cmake
```

從 toolchain presence 與 package-path 驗證結果來看，可以進入後續 configure task。後續 configure 必須在該 process 明確設定 Qt prefix、`PDF4QT_QT_ROOT` 與 vcpkg toolchain path；本安裝腳本刻意不建立全域環境變數。vcpkg manifest dependencies 尚未預先安裝，會由後續 manifest-mode configure/install 階段解析。

## 當時尚未安裝（後續狀態）

- WiX Toolset：未採用；FamilyPDF 後續改用經 Authenticode 驗證的 Inno Setup 7.0.2。
- Tesseract：後續已用獨立 `ocr-spike/vcpkg.json` 安裝 Tesseract 5.5.2，避免改動 PDF4QT 基底 manifest。

## Rollback

所有本 task 的工具都在單一隔離目錄。確認沒有程序正在使用後，刪除以下目錄即可完整 rollback：

```text
E:\CodexProject\FamilyPDF-tools
```

不需要清理全域 `PATH`、Registry 或 system Python，因為本次未修改它們。

## 失敗紀錄與 concerns

- 沙盒內網路預設阻擋 GitHub；依已核准的官方工具下載範圍，安裝改以授權網路執行。
- 初版 vcpkg clone 使用 `--no-checkout`，Git 將空 worktree 顯示為大量 deleted。保護檢查在 bootstrap 前中止；只刪除該新建、未完成的 `E:\CodexProject\FamilyPDF-tools\vcpkg` 後，腳本改為 normal clone 並成功 bootstrap。Qt 與 aqt 目錄未刪除。
- upstream manifest 沒有 `builtin-baseline`；本 task 的 exact vcpkg commit pin 位於安裝腳本。若日後要更新 ports，必須明確更新此 commit、重新驗證依賴，再提交變更。
- aqtinstall 本身已固定 3.3.0，但其 PyPI transitive dependencies 沒有另建 hash lock。它們只用於取得固定 Qt 6.9.1 archives；若需要離線或 byte-for-byte Python environment reproduction，應另建立 wheelhouse 與 hash-locked requirements。
- 本文件記錄的是當時的工具鏈階段；後續 configure、build 與 3/3 測試皆已完成，請見 `upstream-build.md` 與 Phase 1 驗證文件。
