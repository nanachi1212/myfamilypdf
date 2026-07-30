# Microsoft Office 互通性驗證

更新日期：2026-07-30

## 驗證方式

執行：

```powershell
.\scripts\qa\smoke-microsoft-office.ps1
```

腳本會：

1. 使用 FamilyPDF 的 DOCX／XLSX writer 產生固定互通素材。
2. 以 Microsoft Word COM 唯讀開啟 DOCX。
3. 驗證頁數、明確分頁、繁體中文、簡體中文、英文文字，以及粗體／斜體 run。
4. 透過 Word 的 `Range.EnhMetaFileBits` 原生 renderer 輸出第 1、2 頁 PNG 預覽，檢查尺寸及取樣後的非白色像素比例，並記錄 SHA-256。
5. 以 Microsoft Excel COM 唯讀開啟 XLSX。
6. 驗證工作表名稱、表格數值、繁簡中文、逐行 fallback、合併儲存格、UsedRange、表頭粗體與自動欄寬。
7. 不儲存地關閉文件並釋放所有 Office COM 物件。

## 本次結果

- Microsoft Word：16.0，兩頁，多語內容、粗體／斜體與第 2 頁分頁位置完整。
- Word 原生預覽第 1 頁為 3610 × 698、第 2 頁為 3610 × 268；兩張預覽皆通過非空白像素檢查。
- 已巡覽兩張固定 fixture 預覽：繁體、簡體及英文沒有亂碼或缺字方框，粗體、斜體和第二頁文字可辨識，未見裁切或重疊。
- Microsoft Excel：16.0，兩個工作表，表格值、`A1:B1` 合併儲存格、UsedRange、表頭粗體與欄寬完整。
- 結果檔：`build\microsoft-office-smoke\summary.json`。
- 預覽檔：`build\microsoft-office-smoke\word-page-1-preview.png`、`build\microsoft-office-smoke\word-page-2-preview.png`。
- Word／Excel 驗證後沒有殘留行程。
- 測試腳本本身使用 ASCII 與 Unicode code point 組合，不依賴 Windows PowerShell 5.1 對無 BOM UTF-8 原始碼的解碼行為。

## 驗證邊界

這項回歸使用 Microsoft Office 本體的內容、Layout 物件及 Word 原生 renderer，比只讀取 OOXML ZIP 結構更接近實際使用，能抓出空白輸出、分頁、基本樣式、工作表範圍及欄寬問題。固定 fixture 已完成預覽巡覽；任意複雜 PDF 的多欄、浮動圖片、跨頁表格及特殊字型仍需另外由真人評估，且可能需要人工整理。
