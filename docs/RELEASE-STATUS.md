# FamilyPDF 目前交付狀態

更新日期：2026-08-03

## 可直接使用的產物

| 產物 | Bytes | SHA-256 |
|---|---:|---|
| `dist\FamilyPDF-Full-Setup-x64.exe` | 88,029,006 | `9B73EAD2D48A2E872607B3297A383CBEECA5567F8E8E877A5395D14B9CDC46B2` |
| `dist\FamilyPDF-Setup-x64.exe` | 77,143,691 | `FF11B32AF152DE40EBA53481483A55E66712236EA96324453181EC657726EF40` |
| `dist\FamilyPDF-windows-x64.zip` | 101,363,529 | `C24E647DF194B580456916CE13A8271E28553A7221280B6C72FC1EE509A4B6D1` |
| `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe` | 14,049,516 | `C40944A35AEE045DA4C1DC339AD62FB1C63F6F507E562D2EBFECA5773903C801` |
| `dist\FamilyPDF-OCR-Plugin-windows-x64.zip` | 14,879,477 | `C855BB0911C6CFD376A2F11CDCBE5885AACB942A2584272194F6ECE930029BD4` |

`dist\` 是本機建置產物，不提交到 Git。

## 已驗證

- CTest 6/6 通過；另有 Office Export Python 單元測試 8/8 通過。
- pypdf 獨立讀回標準 PDF outline：繁體中文標題、資料夾階層、文字顏色、粗體及頁面目的地均保留。
- Adobe Acrobat DC 已透過官方 IAC 開啟、修改並另存 AcroForm；`pypdf` 獨立讀回確認「姓名」「同意」欄位名稱、修改值與原始預設值保留。修正過程也為 PDF writer 補上 CR／LF hexadecimal string 序列化，避免 UTF-16BE 位元組被外部閱讀器正規化。
- Adobe Acrobat DC 已實際開啟、逐頁解析並另存浮水印／背景與頁面幾何 fixture；`pypdf` 確認頁數、MediaBox、CropBox、旋轉保留，`pypdfium2` 固定倍率逐頁 RGB 像素 SHA-256 與來源完全一致。摘要：`build\acrobat-document-edit-interop\summary.json`。
- 文件級編輯 fixture 已補上頁碼、方向箭頭、外框與四色角標；2026-08-03 以 Poppler 144 DPI 重新渲染五頁並逐頁巡覽，確認單數頁背景、第 2 頁繁中浮水印與 90 度旋轉頁均可讀且無裁切。
- 標準 Highlight、Underline、StrikeOut、Square、FreeText、Text 六種註解由 FamilyPDF 嚴格模式重開及 pypdf 交叉驗證通過。
- 繁體及簡體中文驗證安裝各為 exit code `0`；安裝後 Viewer、Editor、PageMaster 同時載入中文檔名與 1,160 頁 PDF，15 秒後全部為 Responding。
- 單數頁、雙數頁及 `10-20` 範圍輸出經獨立引擎驗證為 29、29、11 頁；合併 58 頁及大型 1,160 頁檔案頁數亦正確。
- 基礎封裝不含 Tesseract、語言模型、測試 EXE 或 Qt 除錯 DLL。
- OCR 外掛 0.3.0 最新隔離安裝 exit code `0`；安裝後 manifest、五個語言模型及橫排／直排 OCR 回歸均通過。
- 一鍵完整安裝程式已分別以 `core,ocr` 與 `core` 元件隔離安裝：完整模式含五個模型並通過繁簡中可搜尋 PDF 回歸，精簡模式不含 OCR，兩者安裝 exit code 均為 `0`。
- 一鍵完整安裝程式已在繁中／簡中互動式 GUI 實測：三種安裝類型及元件說明使用正確語系，完整模式預設勾選 OCR，精簡模式取消 OCR，來回切換可正確恢復元件狀態。
- 基礎版與完整版新增可取消的使用者層級 Windows PDF 整合：繁中／簡中 GUI 均正確顯示且預設勾選；正式安裝器已成功編譯 12 個 HKCU Registry 項目，不改寫預設 `.pdf` ProgID，解除安裝會刪除 FamilyPDF 自有鍵。
- Windows PDF 整合已完成獨立 AppId 的實際安裝／解除安裝 round-trip：安裝與解除安裝 exit code 均為 `0`，四條命令完整引用執行檔與 `%1`，Viewer／Editor 均能開啟含中文及空格的 PDF 路徑並保持 Responding；解除安裝後 Registry 與安裝檔案均已移除。摘要：`build\shell-installer-smoke-summary.json`。
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
- 2026-07-30 可攜包回歸已通過；結果屬本機可重建資料，不提交 Git。
- 最新主安裝程式再次隔離靜默安裝成功；安裝後六個插件及 Office helper 的 DOCX／XLSX 產物重讀通過。
- 修正後的完整驗證安裝檔已重新建置；完整與精簡模式安裝通過，完整模式繁中／簡中／英文 OCR 與可搜尋 PDF 回歸通過。摘要：`build\full-installer-smoke\summary.json`。
- 完整安裝的 405 個核心檔案與 29 個 OCR 檔案、精簡安裝的 405 個核心檔案，均與目前 `dist` 可攜包逐檔 SHA-256 相同；安裝器刻意排除僅供免安裝模式使用的 `portable.mode`。
- Microsoft Word 16.0 已實際開啟匯出 DOCX，辨識三頁、十個段落、明確分頁、繁簡中、英文、五種字級及粗斜體；Word 原生 renderer 另輸出三頁 PNG，尺寸、非白色像素比例及 SHA-256 均已記錄，固定 fixture 巡覽未見亂碼、缺字方框、裁切或重疊。Microsoft Excel 16.0 已實際開啟匯出 XLSX，辨識三個工作表、同頁多表格、四行 fallback、UsedRange、表格值、繁簡中文、跨欄合併、各表表頭粗體及自動欄寬。驗證摘要：`build\microsoft-office-smoke\summary.json`。
- 新增一般等寬雙欄辨識：真實雙欄 PDF 會依左欄由上到下、再右欄由上到下分組，DOCX 以無框線兩欄表格保留可編輯版面；頁頂端跨越欄間中央線的全寬標題會成為表格上方的獨立可編輯段落。表格頁不會被誤判為雙欄，既有單欄行也維持原段落數。來源測試、封裝 EXE 轉換及 `python-docx` 標題／左右欄重讀皆通過。四頁 Word COM fixture 已準備，但本次隔離帳號缺少互動式 Office 工作階段（HRESULT `0x80070520`），不把該項列為已通過。
- 1,160 頁 PDF 已在繁中與簡中 GUI 分別完成第 1 → 580 → 1,160 → 1,157 頁跳轉；頁碼、主畫面與縮略圖同步，簡中視窗持續約 112 秒後仍為 Responding。

## Git

- 分支：`codex/phase0-baseline`
- push 目標：`origin/codex/phase0-baseline`

## 2026-08-03 環境修復與最新回歸

- Office Export 虛擬環境原本仍保留 `python.exe`，但 `pyvenv.cfg` 指向已不存在的 Python 3.14.6。安裝腳本現在會實際啟動 Python 檢查健康狀態，並優先使用相同 ABI 的本機 Python 修復設定；本機已自動改用 vcpkg 內附 Python 3.14.2，`pip check` 與 Office 套件匯入均通過。
- 新增 `scripts\qa\test-office-toolchain-repair.ps1`，覆蓋「啟動器存在、基礎 Python 遺失」的回歸案例；修復失敗時會還原原虛擬環境設定。
- Qt 6.9.1、aqtinstall 3.3.0、vcpkg 固定版本、CMake、MSVC、六個外掛 DLL、PowerShell 腳本語法均重新檢查通過。
- CTest 6/6、Office Export Python 8/8、OCR 橫排與直排繁簡中文、1,160 頁大檔、Viewer／Editor 三文件與繁簡中文回歸均通過。
- 最新完整回歸摘要：`build\final-regression-20260803-122724\summary.json`（本機可重建，不提交 Git）。
- `run-final-regression.ps1` 成功後會自動呼叫安全的保留工具，只留下最新一份完整回歸；測試證明無關目錄不會被刪除。
- Windows 正式封裝以單次多目標執行 `windeployqt`，明確注入 Visual Studio 環境、排除 Qt 6.9.1 會觸發 `0xC0000409` 的 DXC 自動探索，再由 Windows SDK 複製配對的 `dxcompiler.dll`／`dxil.dll`。C++ 測試 runtime 則改用 PE import 整理出的 10 個 release Qt DLL 與 15 個外掛固定 allowlist，連續兩次 CTest 及最終回歸均未出現環境警告或 fallback。
- 本次另移除 13 套已被最新版取代的安裝展開副本，釋放 1,691,093,282 bytes；保留建置樹、最新版安裝驗證及正式 `dist` 產物。加入全寬標題 Office helper 後已重建三個主要產物，最新 SHA-256 如本頁表格。
