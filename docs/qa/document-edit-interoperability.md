# 文件級編輯互通性驗證

更新日期：2026-07-30

## 已完成的自動驗證

- `UnitTestsDocumentEdit` 建立三頁文件，頁面含頁碼、方向箭頭、外框與四色角標；將純色背景套用單數頁、將「家庭測試」文字浮水印套用第 2 頁，並驗證背景在原內容之前、浮水印在原內容之後。
- 同一測試另建立具有相同可辨識內容的頁面尺寸、MediaBox、CropBox 與 90 度旋轉 fixture；由 PDF4QT 寫入磁碟、重開並確認幾何值與內容串流保留。
- `dist\qa\document-edit-interop.pdf` 與 `dist\qa\page-geometry-interop.pdf` 均由 Adobe Acrobat DC 實際開啟、逐頁取得頁面物件並另存。
- Adobe 另存結果由 `pypdf` 驗證頁數、MediaBox、CropBox 與旋轉；再由 `pypdfium2` 以固定倍率逐頁渲染，來源與另存結果的 RGB 像素 SHA-256 完全一致。
- Acrobat 測試拒絕在使用者已有 Acrobat 行程時啟動，並只會清理由隔離測試建立的 Acrobat 行程。
- 2026-08-03 以 Poppler 144 DPI 重新渲染來源 fixture 共五頁並逐頁巡覽：頁碼與 `UP` 方向正確，單數頁背景與第 2 頁繁中浮水印可讀，90 度旋轉頁的外框、角標與文字均未裁切。

執行：

```powershell
.\scripts\qa\smoke-acrobat-document-edit.ps1
```

結果會寫入 `build\acrobat-document-edit-interop\summary.json`。

## 尚待人工巡覽

- 在 FamilyPDF Editor 以不同文字、顏色、圖片與頁碼範圍實際操作每個對話框。
- 由真人依主觀可讀性判斷任意實際文件中的浮水印透明度、背景圖片裁切及複雜原始內容視覺品質。

自動回歸已證明標準 PDF 結構可由 Adobe 讀取／另存，固定 fixture 的渲染畫面不變且最新來源 fixture 已完成逐頁視覺巡覽；它不取代不同實際文件的主觀視覺驗收。
