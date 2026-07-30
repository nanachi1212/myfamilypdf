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
3. 驗證三頁、十個段落、明確分頁、繁體中文、簡體中文、英文文字，以及粗體／斜體與 9／12／14／16／18 pt run。
4. 透過 Word 的 `Range.EnhMetaFileBits` 原生 renderer 輸出第 1 至 3 頁 PNG 預覽，檢查尺寸及取樣後的非白色像素比例，並記錄 SHA-256。
5. 以 Microsoft Excel COM 唯讀開啟 XLSX。
6. 驗證三個工作表、同頁多表格、表格數值、繁簡中文、四行 fallback、跨欄合併、UsedRange、各表表頭粗體與自動欄寬。
7. 不儲存地關閉文件並釋放所有 Office COM 物件。

## 本次結果

- Microsoft Word：16.0，三頁、十個段落，多語內容、五種字級、粗體／斜體與第 2、3 頁分頁位置完整。
- Word 原生預覽第 1 頁為 3610 × 698、第 2 頁為 3610 × 1064、第 3 頁為 3610 × 778；三張預覽皆通過非空白像素檢查。
- 已巡覽三張固定 fixture 預覽：繁體、簡體及英文沒有亂碼或缺字方框，混合字級、粗體、斜體和三頁內容可辨識，未見裁切或重疊。
- Microsoft Excel：16.0，三個工作表；同頁兩個表格、四行 fallback、第三頁摘要、`A1:B1`／`A1:D1` 合併儲存格、UsedRange、各表表頭粗體與欄寬完整。
- 結果檔：`build\microsoft-office-smoke\summary.json`。
- 預覽檔：`build\microsoft-office-smoke\word-page-1-preview.png` 至 `word-page-3-preview.png`。
- Word／Excel 驗證後沒有殘留行程。
- 測試腳本本身使用 ASCII 與 Unicode code point 組合，不依賴 Windows PowerShell 5.1 對無 BOM UTF-8 原始碼的解碼行為。

## 驗證邊界

這項回歸使用 Microsoft Office 本體的內容、Layout 物件及 Word 原生 renderer，比只讀取 OOXML ZIP 結構更接近實際使用，能抓出空白輸出、分頁、混合文字樣式、同頁多表格、工作表範圍及欄寬問題。三頁固定 fixture 已完成預覽巡覽；writer 尚未支援的多欄、浮動圖片、跨頁表格及精確座標版面不在這項證據範圍，特殊字型也仍可能需要人工整理。
