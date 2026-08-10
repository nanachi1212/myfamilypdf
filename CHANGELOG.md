# Changelog

本檔記錄 FamilyPDF 自有功能版本；內嵌的上游 PDF4QT About 版本仍為 1.6.0.0。

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
