# FamilyPDF Repository Instructions

本檔只補充 FamilyPDF 專案特有規則。
一般開發流程、Git/GitHub、安全、驗證與回報方式遵循使用者層 Global `AGENTS.md`。

## Product Identity

- FamilyPDF 是以 PDF4QT 為基底的 Windows x64 家庭用 PDF 工具組。
- 主要使用者是一般 Windows 使用者與家人；優先保持安裝、啟動、閱讀與編輯流程可靠且容易理解。
- 介面與安裝流程需維持繁體中文、簡體中文與英文的既有支援。
- 保留 PDF4QT 原作者署名、MIT License 與所有必要第三方授權資訊；不要因客製化而破壞來源與授權追蹤。

## Scope and Architecture

- 優先在既有 PDF4QT / FamilyPDF 架構內做最小完整修改，避免為單一功能大範圍改寫 upstream 核心。
- Viewer、Editor、PageMaster、Diff、OCR、Office Export、Installer 與 Windows shell integration 是不同交付面；只驗證本次變更真正影響的部分。
- 修改 plugin、文件格式、安裝包或輸出互通性時，先確認實際 consumer 與相容邊界，不要僅依 UI 現象推測底層行為。

## File and Data Safety

涉及 PDF 寫入、加密、解密、備份、復原、表單、簽章、OCR、匯出或檔案整合時：

- 保護原始使用者 PDF 與既有備份。
- 失敗時不可留下看似成功但已損壞的輸出。
- 不建立繞過 PDF 密碼或權限保護的機制。
- 修改安全儲存、復原或外部檔案變更處理時，優先保持可恢復性與既有資料相容。
- 測試不得覆蓋真實使用者文件。

## Windows and Text Files

- 本專案以 Windows 為主要交付平台；優先使用 PowerShell 相容命令與 Windows 路徑語意。
- 編輯既有 source / text file 時保留其既有 line-ending style；對目前使用 CRLF 的檔案不要無關地改成 LF。
- 不因格式化工具造成大量無關 line-ending diff。

## Validation and Builds

依修改範圍選擇最低但充分的驗證，不要求每次都跑完整 FamilyPDF 建置與封裝流程。

- 純文件或 isolated metadata 修改：檢查內容與 diff 即可。
- C++ / Qt source 修改：優先相關編譯或 targeted test；必要時再擴大。
- Viewer / Editor 功能：驗證相關 executable 或自動測試。
- OCR 修改：驗證 OCR plugin / installer 的相關路徑。
- Office Export 修改：驗證對應 helper 與 DOCX/XLSX interoperability。
- Installer / shell integration 修改：才執行相關 installer / shell smoke tests。
- 跨多個正式 plugin、runtime packaging 或 release candidate：再執行完整 regression / packaging。

不要為無關修改無條件執行會下載大型工具鏈或重建所有 installer 的完整流程。

## Context Routing

只有任務涉及對應領域時才讀取相關文件：

- portable / installer：`docs/phase1/portable-package.md`、`docs/phase1/installer.md`
- OCR：`docs/phase1/ocr.md`
- installer GUI / shell integration：`docs/qa/` 下對應文件
- Office interoperability：`docs/qa/office-interoperability.md`
- Acrobat form / document-edit interoperability：對應 `docs/qa/` 文件
- release 狀態：`docs/RELEASE-STATUS.md`

不要因一般局部修改而無條件閱讀整套 QA 與 release 文件。
