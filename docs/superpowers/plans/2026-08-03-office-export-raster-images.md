# Office 匯出 Raster 圖片實作計畫

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 將含可搜尋文字的 PDF 內嵌 raster 圖片依閱讀順序輸出到 Word，支援單欄、雙欄內圖片及跨欄圖片，使用者端不需額外工具。

**Architecture:** `pdfplumber` 提供圖片座標並以已鎖定的 `pypdfium2` 後端在 144 DPI 渲染頁面；抽取器依圖片 bounding box 裁成 PNG。`ExtractedPage.layout_items` 將文字與圖片統一依 `top`、`column`、`column_span` 排序；DOCX writer 將跨欄圖片寫入普通段落、欄內圖片寫入相應表格儲存格，並回報 `images_exported`。

**Tech Stack:** Python 3.14、pdfplumber、pypdfium2、Pillow、pypdf、python-docx、PowerShell QA、PyInstaller。

---

### Task 1: 建立圖片模型與真實 PDF 抽取

**Files:**
- Modify: `office-export/familypdf_office_export/model.py`
- Modify: `office-export/familypdf_office_export/extract.py`
- Modify: `office-export/tests/test_multicolumn_export.py`

- [x] **Step 1: 在雙欄 fixture 加入欄內與跨欄 raster 圖片**

使用 Pillow 建立 100×20 藍色圖片及 120×40 紅色圖片，存入 `BytesIO` PDF，再以 pypdf `merge_transformed_page` 放在左欄第一區段與頁面底端：

```python
def _merge_raster_image(page, size, color, x, y):
    image = Image.new("RGB", size, color)
    image_pdf = BytesIO()
    image.save(image_pdf, format="PDF", resolution=72.0)
    image_pdf.seek(0)
    image_page = PdfReader(image_pdf).pages[0]
    page.merge_transformed_page(
        image_page,
        Transformation().translate(x, y),
    )
```

左欄圖片置於 `(80, 320)`，跨欄圖片置於 `(240, 80)`。

- [x] **Step 2: 新增圖片抽取公開斷言並確認 RED**

```python
self.assertEqual(len(extracted.pages[0].images), 2)
left_image, spanning_image = extracted.pages[0].images
self.assertEqual((left_image.column, left_image.column_span), (0, 1))
self.assertEqual((spanning_image.column, spanning_image.column_span), (0, 2))
self.assertTrue(left_image.data.startswith(b"\x89PNG\r\n\x1a\n"))
self.assertEqual((left_image.width_points, left_image.height_points), (100.0, 20.0))
```

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: FAIL，因為 `ExtractedPage` 尚無 `images`。

- [x] **Step 3: 新增模型與統一閱讀項目**

```python
@dataclass(slots=True)
class ExtractedImage:
    data: bytes
    top: float
    left: float
    width_points: float
    height_points: float
    column: int = 0
    column_span: int = 1

LayoutItem = TextBlock | ExtractedImage
```

`TextBlock` 新增 `left`；`ExtractedPage` 新增 `images` 與 `layout_items`，兩者使用 `default_factory=list` 保持既有呼叫相容。

- [x] **Step 4: 以 144 DPI 頁面渲染裁切 PNG**

```python
rendered = page.to_image(resolution=144).original.convert("RGB")
scale = 2.0
crop = rendered.crop(
    (
        round(x0 * scale),
        round(top * scale),
        round(x1 * scale),
        round(bottom * scale),
    )
)
buffer = BytesIO()
crop.save(buffer, format="PNG")
```

略過寬或高小於 12 points 的裝飾物；渲染失敗時加入頁面及文件 warning，不中止文字匯出。

- [x] **Step 5: 文字與圖片共用區段排序並確認 GREEN**

`_order_layout_items` 對單欄依 `(top, left)` 排序；雙欄以跨欄項目切分區段，每區段依 `(column, top, left)` 排序。`page.blocks` 維持文字專用順序，`page.layout_items` 包含全部文字與圖片。

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Expected: 兩張 PNG、欄位歸屬、尺寸與文字／圖片閱讀順序全部 PASS。

### Task 2: 在 Word 普通段落與雙欄儲存格輸出圖片

**Files:**
- Modify: `office-export/familypdf_office_export/docx_writer.py`
- Modify: `office-export/tests/test_multicolumn_export.py`
- Modify: `office-export/tests/test_cli.py`
- Modify: `scripts/qa/smoke-office-export.ps1`

- [x] **Step 1: 新增 Word 圖片行為斷言並確認 RED**

```python
self.assertEqual(report.images_exported, 2)
self.assertEqual(len(document.inline_shapes), 2)
self.assertEqual(
    [paragraph.text for paragraph in first_cells[0].paragraphs],
    ["Left top", "", "Left bottom"],
)
self.assertEqual(body_order, ["p", "tbl", "p", "tbl", "p"])
```

最後的 `p` 是跨欄圖片段落；其 XML 必須含 `w:drawing`。

- [x] **Step 2: 寫入圖片並限制可用寬度**

```python
def _add_picture(paragraph, image, max_width_emu):
    width = Emu(min(Pt(image.width_points), max_width_emu))
    paragraph.add_run().add_picture(BytesIO(image.data), width=width)
```

普通頁面寬度使用 section page width 扣除左右 margin；雙欄儲存格使用可用寬度除以欄數再乘 0.9。圖片段落不增加 `paragraphs_exported`，只增加 `images_exported`。

- [x] **Step 3: 擴充報告與 CLI 測試**

```python
@dataclass(slots=True, frozen=True)
class DocxExportReport:
    pages_exported: int
    paragraphs_exported: int
    images_exported: int
```

既有純文字 CLI 測試應斷言 `images_exported == 0`；圖片 fixture 應得到 2。

- [x] **Step 4: 單一測試 GREEN 後執行全部 Office 測試**

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest tests.test_multicolumn_export -v`

Run: `E:\CodexProject\FamilyPDF-tools\office-export-venv\Scripts\python.exe -m unittest discover -s tests -v`

Expected: raster 圖片、既有單欄／雙欄／表格／CLI 行為全部 PASS。

- [x] **Step 5: 封裝 helper RED→GREEN**

`smoke-office-export.ps1` 先用舊 helper 驗證缺少兩張圖片而 RED；重建 helper 與可攜包後，以 `python-docx` 驗證兩個 inline shapes、左欄圖片段落、跨欄圖片 body 順序及 `images_exported: 2`。

### Task 3: 正式產物、回歸與交付文件

**Files:**
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 重建可攜包、核心安裝檔與完整安裝檔**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\package-windows-runtime.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-installer.ps1 -SkipPackage -SkipOcr`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage`

- [x] **Step 2: 執行完整／精簡安裝與最終回歸**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1`

Expected: 安裝、OCR、Office、CTest、1,160 頁與多文件回歸全部 PASS；Qt runtime 無警告或 fallback。

- [x] **Step 3: 更新支援邊界、bytes、SHA-256 與回歸路徑**

文件宣告支援非重疊 raster 圖片依閱讀順序輸出；仍不宣告向量圖形、透明遮罩的精確重建、文字環繞／重疊浮動物件、三欄或精確座標排版。

- [x] **Step 4: 提交並嘗試推送**

```powershell
git add docs office-export scripts
git commit -m "feat: preserve raster images in Word export"
git push origin codex/phase0-baseline
```

若沙箱仍無法連線 GitHub 443，保留乾淨且領先遠端的功能分支。
