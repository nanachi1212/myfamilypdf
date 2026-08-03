# Office Export Unequal Columns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 讓 PDF 轉 Word 能辨識並保留常見的不等寬雙欄版面，同時維持標題、圖片、文字順序與既有等寬雙欄行為。

**Architecture:** 擷取器從左右欄文字起點推導實際欄位分界與欄寬比例，將結果存入 `ExtractedPage`；DOCX writer 使用該比例建立固定欄寬的無框線表格。圖片與跨欄內容共用同一條推導分界，避免寬左欄內的內容被錯判成跨欄。

**Tech Stack:** Python 3.14、pdfplumber、pypdf、python-docx、Pillow、PowerShell、PyInstaller。

---

## File structure

- Modify: `office-export/familypdf_office_export/model.py` — 保存每頁欄寬比例與欄位分界。
- Modify: `office-export/familypdf_office_export/extract.py` — 推導不等寬欄位、標記文字與圖片所屬欄位。
- Modify: `office-export/familypdf_office_export/docx_writer.py` — 依比例設定 Word 表格欄寬與圖片最大寬度。
- Modify: `office-export/tests/test_multicolumn_export.py` — 建立不等寬真實 PDF fixture 並驗證 DOCX 結果。
- Modify: `scripts/qa/smoke-office-export.ps1` — 封裝後 helper 的不等寬雙欄回歸。
- Modify: `docs/REQUIREMENTS-AUDIT.md`、`docs/RELEASE-STATUS.md`、`docs/WORKSPACE-HANDOFF.md` — 更新完成範圍、證據與剩餘限制。

### Task 1: 以真實 PDF 建立不等寬雙欄 RED 測試

**Files:**
- Modify: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 新增不等寬雙欄 PDF fixture**

新增 `_write_unequal_two_column_pdf(path)`：600×400 頁面，左欄由 `x=40` 開始、右欄由 `x=440` 開始；加入長左欄文字、右欄文字、橫跨兩欄標題，以及各欄一張圖片。文字內容固定為 `Wide left top`、`Wide left bottom with extended editable text`、`Narrow right top`、`Narrow right bottom`、`Unequal columns heading`。

- [x] **Step 2: 新增 public-behavior 測試**

```python
def test_preserves_unequal_pdf_column_widths_in_docx(self) -> None:
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "unequal-columns.pdf"
        target = Path(directory) / "unequal-columns.docx"
        _write_unequal_two_column_pdf(source)
        extracted = extract_document(source)
        report = write_docx(extracted, target)
        document = Document(target)

    page = extracted.pages[0]
    self.assertEqual(page.column_count, 2)
    self.assertGreater(page.column_width_ratios[0], 0.65)
    self.assertLess(page.column_width_ratios[1], 0.35)
    self.assertEqual([image.column for image in page.images], [0, 1])
    cells = document.tables[0].rows[0].cells
    self.assertGreater(cells[0].width, cells[1].width * 2)
    self.assertEqual(report.images_exported, 2)
```

- [x] **Step 3: 執行單一測試並確認 RED**

Run from `office-export`:

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export.MultiColumnExportTest.test_preserves_unequal_pdf_column_widths_in_docx -v
```

Expected: FAIL，因 `ExtractedPage` 尚未提供 `column_width_ratios`，或 DOCX 仍為等寬欄位。

### Task 2: 擷取欄位分界與欄寬比例

**Files:**
- Modify: `office-export/familypdf_office_export/model.py`
- Modify: `office-export/familypdf_office_export/extract.py`
- Test: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 擴充頁面模型**

在 `ExtractedPage` 新增：

```python
column_width_ratios: list[float] = field(default_factory=lambda: [1.0])
column_split_x: float | None = None
```

- [x] **Step 2: 由欄位起點推導分界與比例**

將 `_group_words_into_blocks` 回傳值改成：

```python
tuple[list[TextBlock], int, list[float], float | None]
```

偵測到雙欄時，以右欄最小起點減去 `page_width * 0.03` 作為 gutter 中央分界；以頁面最左文字起點作對稱頁邊界，計算左右比例並限制在 `0.2..0.8`。所有文字 block 使用同一 `split_x` 判斷 `column`／`column_span`；單欄回傳 `[1.0]` 與 `None`。

- [x] **Step 3: 圖片沿用同一欄位分界**

將 `_extract_images` 增加 `column_split_x` 參數；雙欄時用該值而非固定 `page.width / 2` 判定左欄、右欄或跨欄。`extract_document` 把比例與分界寫入 `ExtractedPage`。

- [x] **Step 4: 執行擷取層測試**

Run:

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export.MultiColumnExportTest.test_preserves_unequal_pdf_column_widths_in_docx -v
```

Expected: 仍可能因 writer 等寬而 FAIL，但欄寬比例與兩張圖片欄位 assertion 已通過。

### Task 3: 使用欄寬比例建立 DOCX

**Files:**
- Modify: `office-export/familypdf_office_export/docx_writer.py`
- Test: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 傳入欄寬比例**

將 `_write_column_table` 簽章改成：

```python
def _write_column_table(
    document,
    columns: list[list[LayoutItem]],
    column_width_ratios: list[float],
) -> tuple[int, int]:
```

比例數量或總和無效時回退等寬；有效時正規化總和。設定 `table.autofit = False`，依可用頁寬設定 `table.columns[index].width` 與每個 `cell.width`，並以該欄寬的 90% 限制欄內圖片。

- [x] **Step 2: 由 `write_docx` 傳入每頁比例**

兩次呼叫 `_write_column_table` 均傳入 `page.column_width_ratios`，跨欄段落仍使用整頁可用寬度。

- [x] **Step 3: 執行單一測試並確認 GREEN**

Run:

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export.MultiColumnExportTest.test_preserves_unequal_pdf_column_widths_in_docx -v
```

Expected: PASS。

- [x] **Step 4: 執行 Office Export 全套來源測試**

Run from `office-export`:

```powershell
E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest discover -s tests -v
```

Expected: 10 tests, 0 failures。

### Task 4: 封裝後回歸與正式交付

**Files:**
- Modify: `scripts/qa/smoke-office-export.ps1`
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 擴充封裝 helper 冒煙測試**

在 `smoke-office-export.ps1` 產生 `unequal-column-input.pdf` 與 `unequal-column-output.docx`；執行 `FamilyPDFOfficeExport.exe`，再以 venv Python 讀回 DOCX，驗證左欄寬至少為右欄兩倍、兩欄文字與兩張圖片均存在。

- [x] **Step 2: 重建 helper 與 Windows 可攜包**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\office\build-office-export-helper.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1 -SkipOfficeBuild
```

Expected: `dist\FamilyPDF-windows-x64\office-export\FamilyPDFOfficeExport.exe` 更新成功且沒有缺失 DLL／Python module。

- [x] **Step 3: 執行封裝後 Office smoke**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-office-export.ps1 -SkipBuild
```

Expected: exit code `0`，不等寬 DOCX 欄寬、文字、圖片驗證通過。

- [x] **Step 4: 重建正式安裝檔並執行完整回歸**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1
```

Expected: CTest、Office 10/10、OCR、安裝隔離、大型 PDF、多文件與文件比較全部通過。

- [x] **Step 5: 更新文件、雜湊與限制**

將「不等寬雙欄」由限制移至已完成範圍；保留三欄以上、浮動／重疊物件、跨頁表格與精確座標版面限制。記錄最新三個正式產物的大小、SHA-256 與回歸摘要路徑。

- [x] **Step 6: 提交並嘗試推送**

```powershell
git add office-export scripts docs
git commit -m "feat: preserve unequal PDF columns in Word"
git push origin codex/phase0-baseline
```

Expected: 本機提交成功；若外部 `github.com:443` 仍不可達，保留乾淨且領先遠端的分支，不重複要求使用者操作。
