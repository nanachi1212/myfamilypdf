# FamilyPDF 目前交付狀態

更新日期：2026-07-30

## 可直接使用的產物

| 產物 | Bytes | SHA-256 |
|---|---:|---|
| `dist\FamilyPDF-Full-Setup-x64.exe` | 69,615,628 | `AD072CAE7559D3F21576BDBFE6E21B8E93CBAB90D767DA6C34BFD8DBF52A732B` |
| `dist\FamilyPDF-Setup-x64.exe` | 58,724,838 | `B0FAAA20B6DFBDE2B22D913C701703A8D09DB60B4CBBF22BCCA807967D6798D0` |
| `dist\FamilyPDF-windows-x64.zip` | 85,970,935 | `E3436FEF229D1A8F5E3B869D7EB3AFB827378C39FD7DAB7812F36003643AC5D2` |
| `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe` | 14,049,516 | `C40944A35AEE045DA4C1DC339AD62FB1C63F6F507E562D2EBFECA5773903C801` |
| `dist\FamilyPDF-OCR-Plugin-windows-x64.zip` | 14,879,477 | `C855BB0911C6CFD376A2F11CDCBE5885AACB942A2584272194F6ECE930029BD4` |

`dist\` 是本機建置產物，不提交到 Git。

## 已驗證

- CTest 6/6 通過；另有 Office Export Python 單元測試 7/7 通過。
- pypdf 獨立讀回標準 PDF outline：繁體中文標題、資料夾階層、文字顏色、粗體及頁面目的地均保留。
- 標準 Highlight、Underline、StrikeOut、Square、FreeText、Text 六種註解由 FamilyPDF 嚴格模式重開及 pypdf 交叉驗證通過。
- 繁體及簡體中文驗證安裝各為 exit code `0`；安裝後 Viewer、Editor、PageMaster 同時載入中文檔名與 1,160 頁 PDF，15 秒後全部為 Responding。
- 單數頁、雙數頁及 `10-20` 範圍輸出經獨立引擎驗證為 29、29、11 頁；合併 58 頁及大型 1,160 頁檔案頁數亦正確。
- 基礎封裝不含 Tesseract、語言模型、測試 EXE 或 Qt 除錯 DLL。
- OCR 外掛 0.3.0 最新隔離安裝 exit code `0`；安裝後 manifest、五個語言模型及橫排／直排 OCR 回歸均通過。
- 一鍵完整安裝程式已分別以 `core,ocr` 與 `core` 元件隔離安裝：完整模式含五個模型並通過繁簡中可搜尋 PDF 回歸，精簡模式不含 OCR，兩者安裝 exit code 均為 `0`。
- 一鍵完整安裝程式已在繁中／簡中互動式 GUI 實測：三種安裝類型及元件說明使用正確語系，完整模式預設勾選 OCR，精簡模式取消 OCR，來回切換可正確恢復元件狀態。
- 基礎版與完整版新增可取消的使用者層級 Windows PDF 整合：繁中／簡中 GUI 均正確顯示且預設勾選；正式安裝器已成功編譯 12 個 HKCU Registry 項目，不改寫預設 `.pdf` ProgID，解除安裝會刪除 FamilyPDF 自有鍵。
- 安裝後 OCR 單頁、雙頁流程通過：頁數不變、原檔 SHA-256 不變、輸出為 PDF、`fetch-text` 可取得文字。
- 橫排繁體中文、簡體中文及英文模型已內建；直接辨識分別取得「傳統中文測試」、「简体中文测试」與 `FamilyPDF OCR 2026`。封裝後端對端流程亦確認原檔雜湊不變、頁數不變、PDF 文字層含繁簡中文字元，且 UTF-8 文字檔保留完整字序。
- 缺少所選語言時，OCR 主流程會自動啟動官方下載、最多三次重試、大小檢查、Tesseract 載入驗證及原子替換；斷線測試確認不產生輸出 PDF 或殘留半檔。
- 直排繁／簡中文已由官方 `tessdata_fast` 成功下載、SHA-256 與 Tesseract 載入驗證通過，並已內建於目前產物。
- `EditorPlugin.dll`、`RedactPlugin.dll`、`SignaturePlugin.dll` 已進入 release 可攜包及正式安裝程式；專用 verification installer 安裝 exit code `0`，安裝後三個 DLL 的大小檢查通過。
- `FormPlugin.dll`、`DocumentEditPlugin.dll`、`OfficeExportPlugin.dll` 與 Office helper 已進入可攜包及正式安裝程式；Office helper 在清除 Python PATH 後完成 DOCX／XLSX 轉換與重讀。
- 最新主安裝程式隔離靜默安裝 exit code `0`；安裝後 Office helper 在無 Python PATH 下完成兩頁 DOCX 匯出。
- Office Export 新增繁體與簡體中文選單、頁碼、進度、錯誤及 OCR 提示；封裝後 `.qm` 已反向轉回 TS 驗證譯文存在。
- 修正舊 PDF4QT 設定含空白插件清單時 FamilyPDF 功能看似遺失的問題；六個正式功能插件現在會在第 2 版設定遷移時自動啟用，後續仍尊重使用者自行停用的選擇。
- GUI 實測已確認繁中介面會顯示三個插件工具列，並可開啟「匯出至 Word」的繁中頁碼範圍對話框。
- Viewer 與 Editor 的命令列多檔輸入已修正為載入全部檔案；兩者均以兩份一般 PDF 加一份 1,160 頁 PDF 回歸，工作階段記錄 3 份文件且維持 Responding。
- 最終可攜包回歸摘要：`build\final-regression-20260730-121838\summary.json`（本機可重建，不提交 Git）。
- 最新主安裝程式再次隔離靜默安裝成功；安裝後六個插件及 Office helper 的 DOCX／XLSX 產物重讀通過。
- Microsoft Word 16.0 已實際開啟匯出 DOCX，辨識兩頁、繁體中文、簡體中文及英文；Microsoft Excel 16.0 已實際開啟匯出 XLSX，辨識兩個工作表、表格值、繁簡中文及合併儲存格。驗證摘要：`build\microsoft-office-smoke\summary.json`。
- 1,160 頁 PDF 已在繁中與簡中 GUI 分別完成第 1 → 580 → 1,160 → 1,157 頁跳轉；頁碼、主畫面與縮略圖同步，簡中視窗持續約 112 秒後仍為 Responding。

## Git

- 分支：`codex/phase0-baseline`
- push 目標：`origin/codex/phase0-baseline`
