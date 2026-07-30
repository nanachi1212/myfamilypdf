# FamilyPDF 表單、Office 匯出與進階編輯 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 讓 FamilyPDF 能填寫及建立標準 AcroForm 表單、啟用並補強 PDF4QT 既有進階編輯能力，並以可選裝外掛將 PDF 文字與表格匯出為可編輯的 DOCX／XLSX。

**Architecture:** 表單與進階編輯留在 C++／Qt 主程式，直接寫入標準 PDF 物件，確保其他閱讀器可保留及繼續編輯。Office 匯出採獨立外掛與 helper process，使用寬鬆授權的 `pypdf`、`pdfplumber`、`python-docx`、`openpyxl`，避免主程式因 Python 套件而膨脹；外掛安裝包自行攜帶 runtime，不要求使用者預裝 Python。所有新能力先以檔案層級自動測試驗證，再製作 Windows 可攜包與安裝程式。

**Tech Stack:** C++20、Qt 6.9、PDF4QT core/widgets/plugin API、Qt Test、Python 3.13、pypdf、pdfplumber、python-docx、openpyxl、PyInstaller、PowerShell、CMake/Ninja、WiX。

---

## 範圍與驗收邊界

- 可填寫表單：
  - 既有 AcroForm 的文字框、核取方塊、單選按鈕、下拉選單與清單可操作、儲存、重開。
  - 可在 PDF 上拖曳建立文字框與核取方塊；後續加入單選群組、下拉選單及清單。
  - 欄位名稱、提示文字、預設值、必填、唯讀、多行等屬性可編輯。
  - 產物是標準 AcroForm，不使用 FamilyPDF 私有 sidecar。
- 進階編輯：
  - 先交付 PDF4QT 已有但尚未打包的內容編輯、文字／圖形／SVG、新增與刪除、復原／重做、遮蔽及簽章外掛。
  - 再補齊浮水印、背景、頁面裁切／尺寸／旋轉等常用文件級操作。
  - 不把「註解文字」冒充「直接修改原始頁面內容」。
- Office 匯出：
  - DOCX 保留可編輯文字、段落、基本字級、粗斜體、頁面順序與分頁；複雜欄位排版可能需要人工微調。
  - XLSX 優先偵測表格，一頁可產生一個工作表；無表格時可選擇逐行輸出。
  - 掃描 PDF 若沒有文字層，UI 會提示先執行 OCR。
  - 第一版不承諾像素級 Word 排版還原，也不包含 Office 巨集、公式重建或圖表語意重建。

## Task 1：建立能力基準與打包檢查

**Files:**
- Modify: `scripts/phase0/build-upstream-baseline.ps1`
- Modify: `scripts/phase0/package-windows-runtime.ps1`
- Create: `scripts/qa/verify-editor-plugins.ps1`
- Modify: `README.md`
- Modify: `docs/phase1/feature-inventory.md`

- [x] Step 1: 建立失敗中的外掛打包檢查

`verify-editor-plugins.ps1` 必須檢查 `EditorPlugin.dll`、`RedactPlugin.dll`、`SignaturePlugin.dll` 存在於 `pdfplugins`；任一缺少就 exit 1。

- [x] Step 2: 執行檢查並確認目前失敗

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/qa/verify-editor-plugins.ps1 -PackageDirectory dist/FamilyPDF-windows-x64`

Expected: 顯示三個缺失 DLL 並回傳非零 exit code。

- [x] Step 3: 將三個外掛加入 release build targets

在 `$Targets` 加入 `EditorPlugin`、`RedactPlugin`、`SignaturePlugin`，保留既有應用程式、測試與翻譯 target。

- [x] Step 4: 建置、打包並重跑檢查

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/phase0/build-upstream-baseline.ps1 -Stage Build`

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/phase0/package-windows-runtime.ps1`

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/qa/verify-editor-plugins.ps1 -PackageDirectory dist/FamilyPDF-windows-x64`

Expected: 三個外掛全部存在，檢查 exit 0。

- [x] Step 5: 更新繁中／簡中功能說明

以繁體中文列出已啟用的直接內容編輯與遮蔽能力，並附上簡體中文使用者也能辨識的英文 menu/action 名稱；明確標示表單建立與 Office 匯出仍在後續階段。

## Task 2：建立標準 AcroForm builder API

**Files:**
- Modify: `Pdf4QtLibCore/sources/pdfdocumentbuilder.h`
- Modify: `Pdf4QtLibCore/sources/pdfdocumentbuilder.cpp`
- Create: `UnitTests/tst_formbuildertest.cpp`
- Modify: `UnitTests/CMakeLists.txt`
- Modify: `scripts/phase0/build-upstream-baseline.ps1`

- [x] Step 1: 先寫表單物件結構測試

測試建立空白 PDF，加入文字欄位與核取方塊後，驗證 Catalog 的 `AcroForm`、`Fields`，以及欄位的 `FT`、`T`、`V`、`DV`、`Ff`、`Rect`、`Subtype=Widget`、頁面 `Annots`。

- [x] Step 2: 執行新測試並確認因 API 不存在而失敗

Run:
`cmake --build build/phase0-upstream-release --target UnitTestsForms --parallel`

Expected: compile failure 指向尚未實作的 builder methods。

- [x] Step 3: 實作最小 builder API

加入：

```cpp
PDFObjectReference createFormFieldText(QString fieldName,
                                       QString defaultValue,
                                       PDFFormField::FieldFlags flags,
                                       std::optional<PDFInteger> maximumLength);
PDFObjectReference createFormFieldCheckBox(QString fieldName,
                                           bool checked,
                                           PDFFormField::FieldFlags flags);
void createFormFieldWidget(PDFObjectReference formField,
                           PDFObjectReference page,
                           QRectF rect,
                           QByteArray defaultAppearance);
PDFObjectReference appendAcroFormField(PDFObjectReference formField);
```

沿用既有 signature builder 的 object factory 寫法；不建立私有 metadata。

- [x] Step 4: 驗證欄位可被 PDF4QT parser 重讀

測試需將 builder document 交給 `PDFFormManager`／`PDFForm::parse`，確認欄位型別、名稱、旗標與值一致。

- [x] Step 5: 建置與執行全套測試

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/phase0/build-upstream-baseline.ps1 -Stage Build`

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/phase0/build-upstream-baseline.ps1 -Stage Test`

Expected: 所有既有測試與 `UnitTestsForms` 通過。

## Task 3：表單建立與欄位管理 UI

**Files:**
- Create: `Pdf4QtEditorPlugins/FormPlugin/CMakeLists.txt`
- Create: `Pdf4QtEditorPlugins/FormPlugin/FormPlugin.json`
- Create: `Pdf4QtEditorPlugins/FormPlugin/formplugin.h`
- Create: `Pdf4QtEditorPlugins/FormPlugin/formplugin.cpp`
- Create: `Pdf4QtEditorPlugins/FormPlugin/formfielddialog.h`
- Create: `Pdf4QtEditorPlugins/FormPlugin/formfielddialog.cpp`
- Modify: `Pdf4QtEditorPlugins/CMakeLists.txt`
- Modify: `scripts/phase0/build-upstream-baseline.ps1`
- Modify: `scripts/qa/verify-editor-plugins.ps1`

- [x] Step 1: 建立 `FormPlugin` 可載入 smoke test

將 `FormPlugin` 加入 CMake，先只回傳「建立文字欄位」「建立核取方塊」「欄位內容反白」「重設表單」四個 action；打包檢查應要求 `FormPlugin.dll`。

- [x] Step 2: 實作拖曳建立欄位

重用 SignaturePlugin 的 page scene／rectangle selection 模式，將畫面座標轉為 PDF page rectangle；完成拖曳後開啟欄位屬性 dialog，再由 builder 寫入文件。

- [x] Step 3: 實作欄位屬性

文字欄位：名稱、提示、預設值、必填、唯讀、多行、最大字數；dialog 直接使用 Qt Widgets 建立，避免多一份 `.ui` 產生檔。  
核取方塊：名稱、提示、預設勾選、必填、唯讀。

- [x] Step 4: 實作既有表單操作

加入反白切換、重設到預設值、Tab 欄位順序，並確保修改會觸發主程式 dirty state。

- [ ] Step 5: 互通性測試

建立含中英文欄位名稱與值的 PDF，儲存後以 PDF4QT parser 重讀；另以至少一個外部閱讀器手動確認文字與核取狀態保留。手動檢查結果寫入 `docs/qa/form-interoperability.md`。

## Task 4：單選與選單欄位

**Files:**
- Modify: `Pdf4QtLibCore/sources/pdfdocumentbuilder.h`
- Modify: `Pdf4QtLibCore/sources/pdfdocumentbuilder.cpp`
- Modify: `Pdf4QtEditorPlugins/FormPlugin/formplugin.cpp`
- Modify: `Pdf4QtEditorPlugins/FormPlugin/formfielddialog.*`
- Modify: `UnitTests/tst_formbuildertest.cpp`

- [x] Step 1: 先寫 radio／combo／list 的 parser round-trip tests
- [x] Step 2: 實作 button group 與 choice field dictionaries、選項與 export values
- [x] Step 3: 在 dialog 支援單選群組、下拉選單、清單及多選設定
- [x] Step 4: 驗證建立、填寫、儲存、重開皆保留值

## Task 5：完成進階編輯常用項目

**Files:**
- Create: `Pdf4QtLibCore/sources/pdfdocumentdecoration.h`
- Create: `Pdf4QtLibCore/sources/pdfdocumentdecoration.cpp`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/CMakeLists.txt`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/DocumentEditPlugin.json`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/documenteditplugin.h`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/documenteditplugin.cpp`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/documenteditdialog.h`
- Create: `Pdf4QtEditorPlugins/DocumentEditPlugin/documenteditdialog.cpp`
- Create: `UnitTests/tst_documentedittest.cpp`
- Modify: `Pdf4QtLibCore/CMakeLists.txt`
- Modify: `Pdf4QtEditorPlugins/CMakeLists.txt`
- Modify: `UnitTests/CMakeLists.txt`
- Modify: `scripts/phase0/build-upstream-baseline.ps1`
- Modify: `scripts/qa/verify-editor-plugins.ps1`

- [x] Step 1: 盤點 EditorPlugin 與既有頁面 API

確認 `EditorPlugin` 已提供文字、圖形、SVG、刪除、undo/redo；頁面幾何直接重用 `PDFPageGeometry` 的頁碼範圍、單雙數、裁切框、頁面尺寸、內容縮放與 annotation 同步，不另建重複資料模型。

- [x] Step 2: 先寫文件裝飾與頁面幾何磁碟 round-trip tests

建立三頁 PDF，先驗證文字浮水印／純色背景只套用指定頁碼及單雙數子集，前景使用 `PlaceAfter`、背景使用 `PlaceBefore`；另驗證裁切框、頁面尺寸與旋轉儲存重開後保留。

- [x] Step 3: 實作 `PDFDocumentDecoration`

支援文字、透明度、角度、字級、顏色、前景／背景、背景色與背景圖片；輸入共用 PDF4QT 頁碼範圍及單雙數子集，成功後回傳精確 modification flags。

- [x] Step 4: 建立 `DocumentEditPlugin` UI

提供「加入文字浮水印」「設定頁面背景」「頁面裁切與尺寸」「向左／向右旋轉」actions；對話框包含頁碼範圍、全部／單數／雙數頁、前景／背景、顏色、圖片、透明度與角度。

- [x] Step 5: 建置、封裝及互通性驗證

將 `DocumentEditPlugin.dll` 加入 release target 與打包檢查；以 PDF4QT parser 及外部 `pypdf` 驗證頁面內容流、頁面框與旋轉均寫入 PDF 本體，最後執行完整 CTest。

## Task 6：Office 匯出 helper 與格式測試

**Files:**
- Create: `office-export/requirements.lock`
- Create: `office-export/familypdf_office_export/__main__.py`
- Create: `office-export/familypdf_office_export/extract.py`
- Create: `office-export/familypdf_office_export/docx_writer.py`
- Create: `office-export/familypdf_office_export/xlsx_writer.py`
- Create: `office-export/tests/test_docx_export.py`
- Create: `office-export/tests/test_xlsx_export.py`
- Create: `office-export/tests/fixtures/README.md`
- Create: `scripts/office/install-office-export-toolchain.ps1`
- Create: `scripts/office/build-office-export-helper.ps1`

- [ ] Step 1: 鎖定 MIT／BSD 相容套件與 hashes

禁止加入 AGPL 或需要商業授權才能重新散布的 PDF library。

- [ ] Step 2: 先寫 DOCX 驗收測試

fixture 包含繁中、簡中、英數、粗體、斜體、雙頁。解壓 DOCX 後驗證 `word/document.xml` 的文字、段落與 page break。

- [ ] Step 3: 實作文字區塊排序與 DOCX writer

以頁面、欄、垂直位置、水平位置排序；保留可合理辨識的段落及基本樣式。

- [ ] Step 4: 先寫 XLSX 驗收測試

fixture 含有框線表格、無框線對齊表格、繁簡中文、合併儲存格；以 openpyxl 重讀並驗證工作表與儲存格。

- [ ] Step 5: 實作表格偵測與 XLSX writer

每頁可建立獨立工作表；無表格時支援逐行 fallback，輸出報告需列出偵測表格數與警告。

- [ ] Step 6: 以 PyInstaller 建立單一 helper 目錄並執行 clean-machine smoke

Run:
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/office/build-office-export-helper.ps1`

Expected: `dist/FamilyPDF-Office-Export/helper/FamilyPDFOfficeExport.exe` 可在未安裝 Python 的測試環境執行。

## Task 7：Office Export Qt 外掛與安裝包

**Files:**
- Create: `Pdf4QtEditorPlugins/OfficeExportPlugin/CMakeLists.txt`
- Create: `Pdf4QtEditorPlugins/OfficeExportPlugin/OfficeExportPlugin.json`
- Create: `Pdf4QtEditorPlugins/OfficeExportPlugin/officeexportplugin.h`
- Create: `Pdf4QtEditorPlugins/OfficeExportPlugin/officeexportplugin.cpp`
- Create: `Pdf4QtEditorPlugins/OfficeExportPlugin/officeexportdialog.*`
- Create: `installer/FamilyPDFOfficeExport.wxs`
- Create: `scripts/office/build-office-export-installer.ps1`
- Create: `scripts/qa/smoke-office-export.ps1`
- Modify: `Pdf4QtEditorPlugins/CMakeLists.txt`

- [ ] Step 1: 加入「匯出成 Word」「匯出成 Excel」及頁面範圍 dialog
- [ ] Step 2: 以 `QProcess` 呼叫 helper，顯示進度、取消、警告與錯誤，不阻塞 UI
- [ ] Step 3: 偵測無文字層頁面並提示先安裝／執行 OCR
- [ ] Step 4: 建立可選裝 Office Export 安裝程式，安裝／移除不影響主程式與 OCR 外掛
- [ ] Step 5: 驗證繁中與簡中 Windows 安裝、DOCX／XLSX 產物重讀

## Task 8：最終回歸、文件、發佈

**Files:**
- Modify: `README.md`
- Modify: `docs/phase1/feature-inventory.md`
- Modify: `docs/RELEASE-STATUS.md`
- Create: `docs/qa/release-checklist.md`

- [ ] Step 1: 執行 CTest、表單互通、Office fixture、1160 頁大檔與多分頁回歸
- [ ] Step 2: 重建主程式、OCR、Office Export 三個安裝包與可攜包
- [ ] Step 3: 記錄版本、大小、SHA256、已知限制及安裝順序
- [ ] Step 4: 僅 stage 本次來源、測試與文件，排除 `dist/`
- [ ] Step 5: commit 並 push `codex/phase0-baseline`

## 建議交付順序與預估

1. Task 1（啟用現成進階編輯）：約 1–2 小時。
2. Task 2–3（文字框／核取方塊建立及既有表單填寫）：約 6–10 小時。
3. Task 4（單選／下拉／清單）：約 4–6 小時。
4. Task 5（浮水印、背景、頁面幾何整合）：約 6–10 小時。
5. Task 6–7（Word／Excel helper、Qt UI、選裝安裝包）：約 16–24 小時。
6. Task 8（完整回歸、雙語安裝、文件及發佈）：約 4–6 小時。

總計約 37–58 個有效工程小時；可先在 Task 1 與 Task 3 各產出一個可用版本，不必等待全部功能完成。
