# 文件級進階編輯互通性驗證

更新日期：2026-07-30

## 已完成的自動驗證

- `UnitTestsDocumentEdit` 建立三頁 PDF，保留每頁原始內容流，再只對單數頁插入背景內容流、只對第二頁插入繁體中文文字浮水印。
- 背景使用標準頁面 `/Contents` 陣列的第一個 stream（`PlaceBefore`）；前景浮水印使用最後一個 stream（`PlaceAfter`），不使用 FamilyPDF 私有 sidecar。
- 圖片背景以 QImage 寫入標準 PDF image resource，支援 Fit、Fill 及 Stretch；測試寫入磁碟後由 PDF4QT reader 重開。
- 頁面幾何測試只修改第二頁為 148 × 210 mm、同步 `MediaBox`／`CropBox` 並順時針旋轉 90 度；第一頁維持 300 × 400 pt 與 0 度。
- 六組 CTest 全數通過，包含新的 `UnitTestsDocumentEdit`。

## 外部 parser 檢查

- `pypdf 6.14.2` 重讀 `dist/qa/document-edit-interop.pdf`，確認三頁的 `/Contents` 都是兩個 stream 的陣列，解碼後總長分別為 358、429、358 bytes。
- `pypdf 6.14.2` 重讀 `dist/qa/page-geometry-interop.pdf`，確認旋轉值為 `[0, 90]`，第二頁尺寸為 `419.52756 × 595.2756 pt`，且兩頁 `CropBox` 均與各自 `MediaBox` 一致。

## 尚待發佈前人工巡覽

- 在 FamilyPDF Editor 的 `Document Edit` 選單逐一操作文字浮水印、純色背景、圖片背景、頁面尺寸與左右旋轉。
- 儲存後以 Microsoft Edge 或 Adobe Acrobat Reader 開啟，確認內容層順序、中文字形、透明度、圖片裁切與頁面方向。
- 在繁體中文及簡體中文 Windows 各測一次預設中文字型與檔案選擇對話框。

人工巡覽完成前，本文件只證明 PDF 結構、磁碟 round-trip 與獨立 parser 互通，不宣稱已完成所有 GUI 驗收。
