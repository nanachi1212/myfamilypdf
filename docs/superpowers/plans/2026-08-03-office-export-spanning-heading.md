# Office 匯出全寬標題實作計畫

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 讓一般雙欄 PDF 頁面頂端的全寬標題在 Word 匯出後維持為可編輯的獨立標題，並保留既有左右欄順序。

**Architecture:** 在文字抽取層以頁面中央線判斷會跨過欄間溝槽的文字區塊，欄位偵測只使用未跨欄的本文區塊。模型以 `column_span` 與 `top` 表達版面角色；DOCX writer 先寫頁首跨欄區塊，再建立既有雙欄表格，避免引入座標式排版或完整頁面重建。

**Tech Stack:** Python 3.14、pdfplumber、pypdf、python-docx、PowerShell QA、PyInstaller。

---

### Task 1: 以公開匯出流程鎖定全寬標題行為

**Files:**
- Modify: `office-export/tests/test_multicolumn_export.py`
- Modify: `office-export/familypdf_office_export/model.py`
- Modify: `office-export/familypdf_office_export/extract.py`

- [x] **Step 1: 在真實 PDF fixture 加入置中的 18pt 全寬標題**

```python
BT /F1 18 Tf 220 375 Td (Full width heading) Tj ET
BT /F1 12 Tf 40 340 Td (Left top) Tj ET
BT /F1 12 Tf 40 300 Td (Left bottom) Tj ET
BT /F1 12 Tf 340 340 Td (Right top) Tj ET
BT /F1 12 Tf 340 300 Td (Right bottom) Tj ET
```

- [x] **Step 2: 新增公開行為斷言並確認 RED**

```python
self.assertEqual(extracted.pages[0].column_count, 2)
heading = extracted.pages[0].blocks[0]
self.assertEqual(heading.text, "Full width heading")
self.assertEqual(heading.column_span, 2)
```

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: FAIL，因為 `TextBlock` 尚無 `column_span`，或標題被錯分到右欄。

- [x] **Step 3: 在模型加入最小版面資訊**

```python
@dataclass(slots=True)
class TextBlock:
    runs: list[TextRun] = field(default_factory=list)
    column: int = 0
    column_span: int = 1
    top: float = 0.0
```

- [x] **Step 4: 欄位偵測排除跨越頁面中央線的區塊**

```python
page_center = page_width / 2.0
column_candidates = [
    position
    for position in block_positions
    if not (position[1] < page_center < position[2])
]
```

偵測到雙欄後，將跨中央線區塊設為 `column_span = 2`；其餘區塊依既有 `split_x` 設定 `column`。所有區塊保留 `top` 供 writer 排序。

- [x] **Step 5: 重跑單一測試確認 GREEN**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: PASS，且頁面仍為雙欄、標題為 `column_span == 2`。

### Task 2: 在 DOCX 中輸出可編輯全寬標題

**Files:**
- Modify: `office-export/familypdf_office_export/docx_writer.py`
- Modify: `office-export/tests/test_multicolumn_export.py`
- Modify: `scripts/qa/smoke-office-export.ps1`

- [x] **Step 1: 新增 DOCX 公開行為斷言並確認 RED**

```python
self.assertEqual(document.paragraphs[0].text, "Full width heading")
self.assertEqual(len(document.tables), 1)
self.assertEqual(cells[0].text.splitlines(), ["Left top", "Left bottom"])
self.assertEqual(cells[1].text.splitlines(), ["Right top", "Right bottom"])
self.assertEqual(report.paragraphs_exported, 5)
```

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: FAIL，因為 writer 目前只在兩個欄位儲存格中尋找區塊。

- [x] **Step 2: writer 先輸出頁首跨欄區塊，再建立雙欄表格**

```python
column_tops = [block.top for block in page.blocks if block.column_span == 1]
first_column_top = min(column_tops, default=float("inf"))
leading_blocks = [
    block
    for block in page.blocks
    if block.column_span > 1 and block.top <= first_column_top
]
```

使用既有 `_write_block` 寫入一般段落；欄位儲存格只接收 `column_span == 1` 的區塊。頁首跨欄段落及兩欄本文都必須計入 `paragraphs_exported`。

- [x] **Step 3: 重跑單一測試確認 GREEN**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: PASS，DOCX 具有一個一般標題段落與一個雙欄表格。

- [x] **Step 4: 擴充封裝 helper smoke**

讓 `smoke-office-export.ps1` 的雙欄 fixture 加入 `Full width heading`，並以 `python-docx` 驗證標題段落、左右儲存格內容及總段落數 5。

- [x] **Step 5: 執行全部 Office 測試與封裝 smoke**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest discover -s tests -v`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\office\build-office-export-helper.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-office-export.ps1 -PackageDirectory dist\FamilyPDF-windows-x64`

Expected: Office Python 8/8 或更多全部 PASS；封裝 helper 產生標題段落與正確雙欄儲存格。

### Task 3: 正式封裝、完整回歸與文件

**Files:**
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 重建正式可攜版與兩種安裝檔**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage`

Expected: 三個正式產物成功重建，且 `windeployqt` 無警告或 fallback。

- [x] **Step 2: 執行安裝與最終回歸**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1`

Expected: 完整／精簡安裝、OCR、Office、CTest 及 1,160 頁多文件回歸全部 PASS；只保留最新回歸資料夾。

- [x] **Step 3: 更新限制與正式產物雜湊**

文件應明確宣告「支援雙欄頁頂端全寬標題」，但仍不宣告支援中段跨欄區塊、三欄、浮動圖片或精確座標排版。更新三個主要產物的 bytes、SHA-256 與最新回歸路徑。

- [x] **Step 4: 提交並嘗試推送**

```powershell
git add docs office-export scripts
git commit -m "feat: preserve full-width headings in Word export"
git push origin codex/phase0-baseline
```

Expected: 本機 commit 成功；若沙箱仍無法連線 `github.com:443`，保留乾淨且領先遠端的分支並記錄外部限制。
