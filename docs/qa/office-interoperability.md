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
4. 以 Microsoft Excel COM 唯讀開啟 XLSX。
5. 驗證工作表名稱、表格數值、繁簡中文、逐行 fallback、合併儲存格、UsedRange、表頭粗體與自動欄寬。
6. 不儲存地關閉文件並釋放所有 Office COM 物件。

## 本次結果

- Microsoft Word：16.0，兩頁，多語內容、粗體／斜體與第 2 頁分頁位置完整。
- Microsoft Excel：16.0，兩個工作表，表格值、`A1:B1` 合併儲存格、UsedRange、表頭粗體與欄寬完整。
- 結果檔：`build\microsoft-office-smoke\summary.json`。
- Word／Excel 驗證後沒有殘留行程。
- 測試腳本本身使用 ASCII 與 Unicode code point 組合，不依賴 Windows PowerShell 5.1 對無 BOM UTF-8 原始碼的解碼行為。

## 驗證邊界

這項回歸使用 Microsoft Office 本體的內容與 Layout 物件，比只讀取 OOXML ZIP 結構更接近實際使用，能抓出分頁、基本樣式、工作表範圍及欄寬問題；但仍不等於人眼評估複雜頁面的版面還原品質。多欄、浮動圖片、跨頁表格及特殊字型仍可能需要人工整理。
