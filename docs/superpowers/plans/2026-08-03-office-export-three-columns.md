# Office Export Three Columns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 讓 PDF 轉 Word 能辨識並保留常見的三欄文件，包含三欄文字順序、欄寬、跨全頁標題與各欄圖片。

**Architecture:** 將目前單一雙欄分界改為最多兩條欄位邊界。擷取器依重複文字起點聚類，選出 2 或 3 個具足夠樣本且間距合理的欄位；模型保存正規化欄寬與邊界，文字、圖片及 DOCX writer 共用同一版面資料。

**Tech Stack:** Python 3.14、pdfplumber、pypdf、python-docx、Pillow、PowerShell、PyInstaller。

---

## File structure

- Modify: `office-export/familypdf_office_export/model.py` — 將單一分界改為欄位邊界清單。
- Modify: `office-export/familypdf_office_export/extract.py` — 以文字起點聚類偵測 2–3 欄並泛化文字／圖片分類。
- Modify: `office-export/tests/test_multicolumn_export.py` — 新增三欄真實 PDF 與 DOCX 行為測試。
- Modify: `scripts/qa/smoke-office-export.ps1` — 驗證封裝 helper 的三欄輸出。
- Modify: `README.md`、`docs/REQUIREMENTS-AUDIT.md`、`docs/RELEASE-STATUS.md`、`docs/WORKSPACE-HANDOFF.md` — 更新功能、限制及正式產物證據。

### Task 1: 建立三欄 RED 測試

**Files:**
- Modify: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 新增三欄真實 PDF fixture**

建立 600×400 PDF：三欄起點為 `x=40`、`x=220`、`x=420`，每欄各有 top／bottom 文字；頁首加入跨越兩條分界的 `Three column heading across page`，三欄各放一張不同顏色 raster 圖片。

- [x] **Step 2: 新增 public-behavior 測試**

```python
def test_preserves_three_pdf_columns_as_editable_docx_columns(self) -> None:
    extracted = extract_document(source)
    report = write_docx(extracted, target)
    document = Document(target)

    page = extracted.pages[0]
    self.assertEqual(page.column_count, 3)
    self.assertEqual(len(page.column_width_ratios), 3)
    self.assertEqual([image.column for image in page.images], [0, 1, 2])
    self.assertEqual(len(document.tables[0].rows[0].cells), 3)
    self.assertEqual(report.paragraphs_exported, 7)
    self.assertEqual(report.images_exported, 3)
```

另外驗證三個 cells 各自包含正確 top／bottom 文字及一個圖片段落，頁首標題為普通可編輯 paragraph。

- [x] **Step 3: 執行單一測試並確認 RED**

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export.MultiColumnExportTest.test_preserves_three_pdf_columns_as_editable_docx_columns -v
```

Expected: FAIL；目前擷取器只能回傳 1 或 2 欄。

### Task 2: 泛化 2–3 欄擷取與輸出

**Files:**
- Modify: `office-export/familypdf_office_export/model.py`
- Modify: `office-export/familypdf_office_export/extract.py`
- Test: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 將模型改為多分界**

以以下欄位取代 `column_split_x`：

```python
column_boundaries: list[float] = field(default_factory=list)
```

- [x] **Step 2: 聚類重複文字起點**

新增 `_cluster_column_starts`：以 `page_width * 0.06` 為容差將 block `x0` 聚類，只保留至少兩個 block 的 cluster。若三個最高樣本 cluster 依水平順序的相鄰間距皆至少 `page_width * 0.22`，選三欄；否則選樣本數最高且間距至少 `page_width * 0.20` 的兩欄。

- [x] **Step 3: 計算欄位邊界與比例**

每條邊界為右側欄起點減 `page_width * 0.03`；以最左欄起點及其對稱右頁界組成各欄寬，正規化為 `column_width_ratios`。單欄回傳 `[1.0]` 與空 boundaries。

- [x] **Step 4: 泛化文字與圖片分類**

跨越全部 boundaries 的項目標記為 `column=0`、`column_span=column_count`；其他項目依中心點落在幾條 boundary 右側決定 `column`。`_extract_images` 接收 boundaries 清單，不再使用單一 `column_split_x`。

- [x] **Step 5: 執行三欄測試並確認 GREEN**

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export.MultiColumnExportTest.test_preserves_three_pdf_columns_as_editable_docx_columns -v
```

Expected: PASS。

- [x] **Step 6: 執行 Office Export 全套來源測試**

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest discover -s tests -v
```

Expected: 11 tests, 0 failures；既有單欄、等寬雙欄、不等寬雙欄、圖片、表格與 CLI 均不退步。

### Task 3: 封裝、正式回歸與提交

**Files:**
- Modify: `scripts/qa/smoke-office-export.ps1`
- Modify: `README.md`
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 加入封裝 helper 三欄冒煙測試**

產生 `three-column-input.pdf`、輸出 `three-column-output.docx`，讀回驗證一個三欄表格、三欄文字、三張圖片、跨欄標題及 `images_exported: 3`。

- [x] **Step 2: 重建 helper、可攜包及正式安裝檔**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\office\build-office-export-helper.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1 -SkipOfficeBuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1
```

Expected: 三個正式產物重建成功，Qt 固定 runtime 封裝沒有環境警告。

- [x] **Step 3: 執行完整回歸與隔離安裝**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1
```

Expected: CTest 6/6、Office 11/11、封裝 helper 三欄、OCR、PDF 比較、多文件及完整／精簡安裝全部通過。

- [x] **Step 4: 更新文件與正式產物雜湊**

將「三欄以上」限制縮小為「四欄以上」；記錄最新三個主要產物 bytes、SHA-256 與回歸摘要路徑。

- [x] **Step 5: 提交並嘗試推送**

```powershell
git add office-export scripts README.md docs
git commit -m "feat: preserve three-column PDF layouts in Word"
git push origin codex/phase0-baseline
```

Expected: 本機提交成功；若 `github.com:443` 仍不可達，保留乾淨且領先遠端的分支。
