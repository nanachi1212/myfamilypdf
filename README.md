# FamilyPDF 家庭用 PDF 閱讀器

FamilyPDF 是以開源 PDF4QT 為基底製作的免費 Windows x64 PDF 工具組，供自己與家人使用。安裝與程式介面支援繁體中文、簡體中文及英文。

## 已完成的主要功能

- 閱讀、搜尋、列印與開啟數百頁 PDF。
- 多色反白、底線、刪除線、波浪線、框選、自由文字與註解側欄。
- 書籤建立、導入、自動產生、文字顏色與可收合資料夾，以及跨 Viewer／Editor 重啟保存。
- PDF 合併與拆分；合併前可選單數頁、雙數頁或輸入 `1-3,8,10-12` 等頁碼範圍。
- Viewer／Editor 可一次開啟多份 PDF；每份文件使用獨立視窗，所有視窗頂端都有同步文件分頁可快速切換，並支援工作階段還原、縮圖、頁碼跳轉與完整縮放操作。
- 安全儲存、外部檔案變更偵測、自動復原快照與最多三份隱藏備份。
- 進階內容編輯外掛：直接編輯頁面內容、新增文字／圖形／SVG、刪除、復原／重做、永久遮蔽及電子／數位簽章。請在 `Pdf4QtEditor.exe` 的 `Editor`、`Redact`、`Signature` 選單使用；英文 action 名稱在繁體／簡體 Windows 相同。
- 文件級進階編輯：在 `Document Edit` 選單可依全部／單數／雙數／指定頁碼加入文字浮水印、純色或圖片背景、調整頁面尺寸與裁切框、縮放內容及向左／向右旋轉。
- 標準 AcroForm 表單：可填寫既有表單，也可在 `Pdf4QtEditor.exe` 的 `Forms` 選單拖曳建立文字框、核取方塊、單選按鈕群組、下拉選單與清單；支援名稱／提示／預設值／必填／唯讀／多行／最大字數／清單多選、反白欄位及重設表單。
- Office 匯出：在 `Pdf4QtEditor.exe` 的 `Office Export` 選單將全部或指定頁碼的可搜尋文字匯出成可編輯 DOCX／XLSX；掃描檔會提示先執行 OCR。
- OCR 可由完整安裝程式一次安裝，也保留獨立外掛封裝；基礎安裝程式不夾帶 OCR 執行環境。
- 免管理員權限的 Windows 安裝程式，以及免安裝可攜式 ZIP。

## 直接使用

本機建置產物：

- `dist\FamilyPDF-Full-Setup-x64.exe`：自己與家人建議使用；預設一次安裝主程式與 OCR，也可取消 OCR。
- `dist\FamilyPDF-Setup-x64.exe`：不需要 OCR 時使用的較小基礎安裝程式。
- `dist\FamilyPDF-windows-x64.zip`：解壓縮整個資料夾後使用。

主要程式：

- `Pdf4QtViewer.exe`：閱讀 PDF。
- `Pdf4QtEditor.exe`：編輯、標記、打字與註解。
- `Pdf4QtPageMaster.exe`：合併、拆分與頁面整理。
- `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe`：OCR 外掛安裝程式。
- `dist\FamilyPDF-OCR-Plugin-windows-x64.zip`：OCR 外掛可攜式覆蓋包。

五種 AcroForm 欄位、文件級進階編輯與 PDF 轉 DOCX／XLSX 已完成自動測試；安裝包內六個正式功能插件在首次啟動或舊設定升級時會自動啟用。仍需在兩台分別使用繁體／簡體 Windows 的實機進行長時間人工操作巡覽。

詳細操作與驗證：

- [可攜式包使用方式](docs/phase1/portable-package.md)
- [Windows 安裝檔](docs/phase1/installer.md)
- [OCR 使用方式](docs/phase1/ocr.md)
- [功能驗證結果](docs/phase1/functional-verification.md)
- [1,160 頁 PDF 互動與語系驗證](docs/qa/large-pdf-interaction.md)
- [Microsoft Office 互通性驗證](docs/qa/office-interoperability.md)
- [目前交付狀態與 SHA-256](docs/RELEASE-STATUS.md)

## 建置

在 Windows PowerShell 執行：

```powershell
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-upstream-baseline.ps1 -Stage All
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\package-windows-runtime.ps1 -SkipOcr
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\ocr\build-ocr-plugin.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\ocr\build-ocr-installer.ps1 -SkipPackage
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\office\build-office-export-helper.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\qa\run-final-regression.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\qa\smoke-full-installer.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\qa\smoke-large-pdf-locales.ps1
& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File .\scripts\qa\smoke-microsoft-office.ps1
```

建置腳本會在缺少時自動安裝 Qt／vcpkg 建置工具；安裝程式腳本會下載並驗證 Inno Setup 的 Authenticode 簽章。開發工具預設放在 `E:\CodexProject\FamilyPDF-tools`。

## 開源基底

FamilyPDF 保留 PDF4QT 的 MIT License 與第三方授權資訊。以下是原始專案說明。

[![CI](https://github.com/JakubMelka/PDF4QT/actions/workflows/ci.yml/badge.svg)](https://github.com/JakubMelka/PDF4QT/actions/workflows/ci.yml)

# PDF4QT

**(c) Jakub Melka 2018-2025**

**Mgr.Jakub.Melka@gmail.com**

**https://jakubmelka.github.io/**

This software is consisting of PDF rendering library, and several
applications, such as advanced document viewer, command line tool,
and document page manipulator application. Software is implementing PDF
functionality based on PDF Reference 2.0. It is written and maintained
by Jakub Melka.

*Software works on Microsoft Windows / Linux.*

Software is provided without any warranty of any kind.

Should you find this software beneficial, your support would be greatly appreciated [:heart: Sponsor](https://github.com/sponsors/JakubMelka)!

## 1. ACKNOWLEDGEMENTS

This software is based in part on the work of the Independent JPEG Group.

Portions of this software are copyright © 2019 The FreeType
Project (www.freetype.org). All rights reserved.

## 2. LEGAL ISSUES

This software was originally licensed under the GNU Lesser General Public License version 3 (LGPLv3).
As of April 27, 2025, the project has been relicensed under the MIT License by the original author.
The change to the MIT License was made to provide greater freedom and flexibility for both open-source and commercial use, reduce legal complexity, and encourage broader adoption and contribution.

Please see the attached LICENSE.txt file for details.

This software also uses several third-party libraries, and users must comply with the licenses of those third-party components.

## 3. FEATURES

Software have following features (the list is not complete):

- [x] multithreading support
- [x] encryption
- [x] color management
- [x] optional content handling
- [x] text layout analysis
- [x] signature validation
- [x] annotations
- [x] form filling
- [x] text to speech capability
- [x] editation
- [x] file attachments
- [x] optimalization (compressing documents)
- [x] command line tool
- [x] audio book conversion
- [x] internal structure inspector
- [x] compare documents
- [x] static XFA support (readonly, simple XFA only)
- [x] electronically/digitally sign documents
- [x] public key security encryption

## 4. THIRD PARTY LIBRARIES

Several third-party libraries are used.

1. libjpeg, see https://www.ijg.org/
2. FreeType, see https://www.freetype.org/index.html, FTL license used
3. OpenJPEG, implementing Jpeg2000, see https://www.openjpeg.org/, 2-clause MIT license
4. Qt, https://www.qt.io/, LGPL license used
5. OpenSSL, https://www.openssl.org/, Apache 2.0 license
6. LittleCMS, http://www.littlecms.com/
7. zlib, https://zlib.net/
8. Blend2D, https://blend2d.com/

## 5. CONTRIBUTIONS

Contributions are welcome!

Since the project is now licensed under the MIT License, contributions can be freely submitted without the need to sign a Contributor License Agreement (CLA).
However, all contributions must be made under the terms of the MIT License to ensure license consistency across the project.

You are encouraged to contribute by testing, offering feedback, providing advice, or submitting code improvements.

## 6. INSTALLING

### Windows

The [Release page](https://github.com/JakubMelka/PDF4QT/releases) lists binaries for Windows, both with and without an installer.

### Arch Linux

A [pdf4qt-git](https://aur.archlinux.org/packages/pdf4qt-git) package is available in the AUR.

### Linux - Flatpak/AppImage

For other Linux distributions, there are two options available. A Flatpak package can be accessed at [Flathub](https://flathub.org/apps/io.github.JakubMelka.Pdf4qt).
Alternatively, an AppImage is available in the Releases section. The AppImage format is designed to work on nearly all Linux systems.
Historically, a .deb package was also offered, but it has been discontinued due to compatibility issues with some Linux distributions.
The executable names are: Pdf4QtEditor, Pdf4QtDiff, Pdf4QtLaunchPad, Pdf4QtPageMaster, Pdf4QtViewer, and PdfTool.

## 7. COMPILING

This software can be compiled on both Windows and Linux. A compiler supporting the C++20 standard is needed.

On Windows, you can use Visual Studio 2022 or MinGW.

On Linux, a GCC version >= 8 should work, altough we tested it with GCC 11.

### Compiling from sources

1. Install [vcpkg](https://vcpkg.io/en/getting-started.html)

        git clone https://github.com/Microsoft/vcpkg.git
        ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
        VCPKG_ROOT=$(pwd)/vcpkg

    Check that vcpkg path is correct: `$VCPKG_ROOT/vcpkg --version`.

2. Build PDF4QT

    2.1 Clone repo

        git clone https://github.com/JakubMelka/PDF4QT
        cd PDF4QT

    2.2 Configure

        cmake -B build -S . -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_INSTALL_PREFIX='/' -DCMAKE_BUILD_TYPE=Release

   (One user reported success with `-DCMAKE_INSTALL_PREFIX=''` instead, as otherwise all there paths were prepended with `/usr` (causing `/usr/usr...`

    For a debug build, append `-DCMAKE_BUILD_TYPE=Debug`.

    It is recommended to set the VCPKG_OVERLAY_PORTS variable to 'PDF4QT/vcpkg/overlays' to prevent crashes due to the incompatible LIBPNG library on some Linux systems.

    2.3 Build

        cmake --build build

    Use the [`-j` switch](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-build-j) to build multiple files in parallel.

    2.4 Install

        sudo cmake --install build

    To uninstall, run `sudo xargs rm < ./build/install_manifest.txt`.

### Using Qt Creator (both Windows/Linux)
1. Download Qt 6.9 or higher, and VCPKG package manager (https://vcpkg.io/en/index.html)
2. Open Qt Creator and configure the project
3. Build
 
### CMAKE Compilation Options

Several important compilation options are available and should be set before building. On Windows,
CMake can prepare a Wix project to create a *.msi installer package.

|                  Option                | Platform |     Description                                          |
| ------------------------------------   | ---------|--------------------------------------------------------- |
| `PDF4QT_INSTALL_MSVC_REDISTRIBUTABLE`  | Windows  |Includes MSVC redistributable in installation             |
| `PDF4QT_INSTALL_PREPARE_WIX_INSTALLER` | Windows  |Prepare .msi installator using Wix installer              |
| `PDF4QT_INSTALL_DEPENDENCIES`          | Any      |Install dependent libraries into installation directory   |
| `PDF4QT_INSTALL_QT_DEPENDENCIES`       | Any      |Install Qt dependent libraries into installation directory|
| `VCPKG_OVERLAY_PORTS`                  | Linux    |Set it to prevent crashes with incompatible libpng library|
 
Following important variables should be set or checked before any attempt to compile this project:

|                  Variable              | Platform |     Description                                          |
| ------------------------------------   | ---------|--------------------------------------------------------- |
| `PDF4QT_QT_ROOT`                       | Any      |Qt installation directory                                 |
| `QT_CREATOR_SKIP_VCPKG_SETUP`          | Any      |Enable or disable automatic vcpkg setup                   |
| `CMAKE_PROJECT_INCLUDE_BEFORE`         | Any      |Should be set to package manager auto setup               |
| `CMAKE_TOOLCHAIN_FILE`                 | Any      |Should be set to toolchain                                |
| `CMAKE_BUILD_TYPE`                     | Any      |Can be Release (default) or Debug                         |

#### Sample setup on Windows

Following set of variables gives sample setup for MS Windows. It is minimal initial configuration
to be able to built Debug build on MS Windows.

| Key                             | Value                                                        |
| ------------------------------- | -------------------------------------------------------------|
| `CMAKE_BUILD_TYPE`              | Debug                                                        |
| `CMAKE_CXX_COMPILER`            | %{Compiler:Executable:Cxx}                                   |
| `CMAKE_C_COMPILER`              | %{Compiler:Executable:C}                                     |
| `CMAKE_GENERATOR`               | Ninja                                                        |
| `CMAKE_PREFIX_PATH`             | %{Qt:QT_INSTALL_PREFIX}                                      |
| `CMAKE_PROJECT_INCLUDE_BEFORE`  | %{IDE:ResourcePath}/package-manager/auto-setup.cmake         |
| `CMAKE_TOOLCHAIN_FILE`          | %{Qt:QT_INSTALL_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake     |
| `PDF4QT_QT_ROOT`                | C:/Programming/Qt/6.4.0/msvc2019_64                          |
| `QT_QMAKE_EXECUTABLE`           | %{Qt:qmakeExecutable}                                        |


### Tested Compilers - Windows
 - Visual Studio 2022 (Microsoft Visual C++ Compiler 17.1)
 - MinGW 11.2.0
 
### Tested Compilers - Linux
 - GCC 13.1.1

## 8. DISCLAIMER

I wrote this project in my free time. I hope you will find it useful!
