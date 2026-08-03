# Office 匯出中段跨欄區段實作計畫

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 讓雙欄 PDF 中位於頁面頂端、中段或底端的全寬文字區塊，在 Word 中依原本垂直順序分隔多個可編輯雙欄區段。

**Architecture:** 抽取層以 `TextBlock.top`、`column` 與 `column_span` 建立閱讀順序：每遇到跨欄區塊，就先輸出其上方左欄再右欄的本文，接著輸出跨欄區塊。DOCX writer 依這個公開模型逐段 flush 雙欄表格，跨欄文字則寫成普通段落；不導入座標式 Word 物件。

**Tech Stack:** Python 3.14、pdfplumber、pypdf、python-docx、PowerShell QA、PyInstaller。

---

### Task 1: 以真實 PDF 鎖定中段跨欄閱讀順序

**Files:**
- Modify: `office-export/tests/test_multicolumn_export.py`
- Modify: `office-export/familypdf_office_export/extract.py`

- [x] **Step 1: 擴充 fixture 為兩個雙欄區段**

在第一組左右欄下方加入：

```python
BT /F1 16 Tf 220 250 Td (Middle heading) Tj ET
BT /F1 12 Tf 40 210 Td (Left second top) Tj ET
BT /F1 12 Tf 40 170 Td (Left second bottom) Tj ET
BT /F1 12 Tf 340 210 Td (Right second top) Tj ET
BT /F1 12 Tf 340 170 Td (Right second bottom) Tj ET
```

- [x] **Step 2: 新增抽取閱讀順序斷言並確認 RED**

```python
self.assertEqual(
    [block.text for block in extracted.pages[0].blocks],
    [
        "Full width heading",
        "Left top",
        "Left bottom",
        "Right top",
        "Right bottom",
        "Middle heading",
        "Left second top",
        "Left second bottom",
        "Right second top",
        "Right second bottom",
    ],
)
```

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: FAIL；舊排序會把兩個跨欄標題都放在所有欄本文之前。

- [x] **Step 3: 依跨欄區塊切分閱讀區段**

在 `_group_words_into_blocks` 偵測到雙欄後，以 `top` 排序跨欄區塊；對每個跨欄區塊，先加入前一個邊界與目前 `top` 之間的欄本文（`column`, `top`, `x0` 排序），再加入跨欄區塊，最後加入剩餘欄本文。

```python
ordered_positions: list[tuple[TextBlock, float, float, float]] = []
previous_top = float("-inf")
for spanning in spanning_positions:
    ordered_positions.extend(
        sorted(
            (
                item
                for item in column_positions
                if previous_top < item[3] < spanning[3]
            ),
            key=lambda item: (item[0].column, item[3], item[1]),
        )
    )
    ordered_positions.append(spanning)
    previous_top = spanning[3]
```

- [x] **Step 4: 重跑單一測試確認抽取 GREEN**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: 抽取順序斷言通過；DOCX 斷言仍會因只有一個雙欄表格而失敗。

### Task 2: 依閱讀順序建立多個可編輯雙欄區段

**Files:**
- Modify: `office-export/familypdf_office_export/docx_writer.py`
- Modify: `office-export/tests/test_multicolumn_export.py`
- Modify: `scripts/qa/smoke-office-export.ps1`

- [x] **Step 1: 新增 DOCX 結構斷言並確認 RED**

```python
self.assertEqual(
    [paragraph.text for paragraph in document.paragraphs if paragraph.text],
    ["Full width heading", "Middle heading"],
)
self.assertEqual(len(document.tables), 2)
self.assertEqual(report.paragraphs_exported, 10)
```

兩個表格的左右儲存格分別必須等於第一組與第二組左右欄文字。

- [x] **Step 2: writer 遇到跨欄區塊時 flush 目前欄本文**

```python
pending_columns: list[list[TextBlock]] = [
    [] for _ in range(page.column_count)
]
for block in page.blocks:
    if block.column_span > 1:
        paragraphs_exported += _write_column_table(
            document, pending_columns
        )
        pending_columns = [[] for _ in range(page.column_count)]
        paragraph = document.add_paragraph()
        _write_block(paragraph, block)
        paragraphs_exported += 1
    else:
        pending_columns[block.column].append(block)
paragraphs_exported += _write_column_table(document, pending_columns)
```

`_write_column_table` 在所有欄都為空時不建立表格；其餘情況沿用現有 cell paragraph 寫入規則並回傳實際段落數。

- [x] **Step 3: 單一測試 GREEN 後執行全部 Office 測試**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest discover -s tests -v`

Expected: 中段標題、兩個雙欄表格及 10 個可編輯段落通過；Office Python 8/8 全部 PASS。

- [x] **Step 4: 擴充封裝 helper 重讀並確認 GREEN**

`smoke-office-export.ps1` 應驗證兩個普通標題段落、兩個雙欄表格、兩組左右欄內容；先用舊 helper 確認 RED，再重建正式 helper／可攜包後確認 GREEN。

### Task 3: 正式產物、完整回歸與交付記錄

**Files:**
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 重建可攜包、核心安裝檔與完整安裝檔**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage`

Expected: 三個產物重建成功，正式封裝無 Qt 警告或 fallback。

- [x] **Step 2: 執行安裝與完整回歸**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1`

Expected: 完整／精簡安裝、OCR、Office、CTest、1,160 頁與多文件回歸全部 PASS，只保留最新回歸結果。

- [x] **Step 3: 更新支援範圍、bytes、SHA-256 與回歸路徑**

文件宣告一般雙欄頁的頂端／中段／底端全寬文字可依順序重建；仍不宣告不等寬、三欄以上、浮動圖片、跨頁表格或精確座標排版。

- [x] **Step 4: 提交並嘗試推送**

```powershell
git add docs office-export scripts
git commit -m "feat: preserve spanning sections in Word export"
git push origin codex/phase0-baseline
```

Expected: 本機 commit 成功；若 GitHub 443 仍受沙箱阻擋，保留乾淨且領先遠端的分支。
