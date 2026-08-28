# Changelog

本檔記錄 FamilyPDF 自有功能版本；內嵌的上游 PDF4QT About 版本仍為 1.6.0.0。

## [0.2.3-beta] - 2026-08-27

### Added

- Inno Setup 覆蓋升級契約：沿用既有 FamilyPDF `AppId`、安裝目錄、語言、元件與工作選項，不要求先解除安裝 0.2.2。
- 書籤原子替換失敗回歸、外部程式 timeout 契約與工具鏈路徑可攜性驗證。

### Changed

- Office Export 與 Windows WIA Scanner 加入可設定 watchdog，取消或逾時時會終止外部程序。
- FamilyPDF 工具根目錄預設從 repository 同層解析，也可用 `FAMILYPDF_TOOLS_ROOT` 覆寫。
- Editor、Reader、PageMaster 與 Diff 的顯示名稱和內部設定名稱統一改為 FamilyPDF；本版不搬移舊 PDF4QT 偏好設定。

### Fixed

- 修正文字轉語音狀態斷言使用賦值運算子。
- 書籤 JSON 改用 `QSaveFile` 原子提交，寫入失敗時保留原檔。
- 簽署 PDF 與附件輸出改用 `QSaveFile` 原子提交，避免直接截斷使用者輸出檔。
- 文字轉語音的 proxy、語音引擎、播放文件與同步控制項加入 Release 建置仍有效的 null guard。
- 文字轉語音在 UI 尚未初始化或設定指標為空時不再更新不存在的控制項；空設定也會安全忽略。
- `UnitTestsBookmarks` 固定使用 Qt `offscreen` 平台並部署對應 plugin，避免測試啟動時出現 Qt platform plugin 初始化錯誤。
- 測試 CMake target 的共用輸出、連結與 CTest 設定集中到 `add_familypdf_qt_test()`，降低重複設定漂移。
- Office Export DOCX／XLSX writer 改用同目錄暫存檔與原子替換，並新增寫入路徑回歸測試。
- recovery snapshot 與 `PDFSafeSaveService` 的 POSIX 覆蓋提交改用 `std::rename` 原子替換，避免先刪除目標檔造成資料遺失窗口。
- vcpkg manifests 與 PDF4QT CMake 版本統一為 1.6.0.0。
- AES 解密改為嚴格拒絕截斷、非區塊對齊及錯誤 PKCS#7 padding，並新增 AES-256 邊界／亂數 IV／錯誤輸入回歸測試。
- Appx、WiX、Debian、Flatpak、AppStream 與 desktop metadata 的可見產品名稱、版本與來源統一為 FamilyPDF；為保留既有 Windows／Linux 升級識別，技術 package ID 維持原值。
- 新增跨平台 metadata 與 AES 安全契約，接入 GitHub validation；新增 1160 頁 PDF 的繁中／簡中 Windows runtime 啟動、回應性與記憶體採樣 smoke。
- installer 編譯腳本支援指定 runtime package 路徑，避免同步工具鎖定舊套件時無法產生可驗證的新 installer。

## [0.2.2-beta] - 2026-08-10

### Added

- `PdfTool`、Tesseract 非零退出與 sidecar 鎖檔的故障注入回歸。
- Qt 6.9.1 官方 SPDX SBOM、Office dependency lock 與 notices 封裝驗證。
- `OCR_VERSION` 與 GitHub release metadata consistency gate。

### Changed

- OCR 外掛更新為 0.4.2；主程式與 OCR installer／manifest 分別只讀取 `VERSION` 與 `OCR_VERSION`。

### Fixed

- PDF 已發布後 sidecar 或頁面圖片寫入失敗會留下不完整 OCR 結果；現在所有輸出先完成 staging，發布失敗時會還原原輸出。

## [0.2.1-beta] - 2026-08-10

### Added

- 固定語言模式的逐頁 JSON 報告。
- OCR 模型 manifest、下載雜湊／半檔清理測試與 Windows GitHub validation workflow。
- `-Pages`、`-KeepPageImages`、損壞模型自動修復及輸出目錄誤用回歸。

### Changed

- OCR 外掛更新為 0.4.1；五個官方模型固定至 `tessdata_fast` commit `87416418657359cb625c412a48b6e1d6d41c29bd`。
- JSON 報告不再記錄輸入或輸出檔名。

### Fixed

- 固定語言模式指定 `OutputReport` 時 `pages` 為空陣列。
- 已損壞但仍存在的模型未觸發自動修復。
- 輸出路徑實際是目錄時，OCR 已產生部分結果才失敗。

## [0.2.0-beta] - 2026-08-10

### Added

- OCR `Auto` 模式逐頁判斷繁體、簡體或混合中文，低信心時再比較版面模式。
- 每頁語言、版面模式、信心與人工複核提示的 JSON 報告。
- 繁體、簡體、混合與空白頁回歸，以及 OCR 外掛隔離安裝／解除安裝測試。
- 主程式與 OCR 套件的第三方授權與 notices。

### Changed

- FamilyPDF 安裝程式版本更新為 0.2.0；OCR 外掛更新為 0.4.0。
- 自動報告只記錄輸入與輸出檔名，不寫入使用者絕對路徑。

### Fixed

- 禁止輸入 PDF、輸出 PDF、文字輸出與 JSON 報告使用相同路徑，避免覆寫來源或其他結果。
- PowerShell 5.1 的空白選用輸出路徑與繁體中文字元解析相容性。

[0.2.0-beta]: https://github.com/nanachi1212/myfamilypdf/releases/tag/v0.2.0-beta
[0.2.1-beta]: https://github.com/nanachi1212/myfamilypdf/releases/tag/v0.2.1-beta
[0.2.2-beta]: https://github.com/nanachi1212/myfamilypdf/releases/tag/v0.2.2-beta
[0.2.3-beta]: https://github.com/nanachi1212/myfamilypdf/releases/tag/v0.2.3-beta
