# FamilyPDF OCR

## 已實作範圍

- 採用 Tesseract 5 與 `tessdata_fast`。
- 預設辨識繁體中文與英文：`chi_tra+eng`。
- 可改用簡體中文與英文：`chi_sim+eng`。
- 可處理整份 PDF，或用 `-Pages` 指定頁面，例如 `1-3,8,10-12`。
- 原始 PDF 保持不變，辨識結果輸出成 UTF-8 純文字檔。
- 封裝腳本會自動下載缺少的 OCR 語言資料與編譯相依套件。

第一版刻意不直接覆寫 PDF，也不把每頁點陣化後冒充原始文件。這可避免破壞向量文字、既有註解、表單與書籤。後續的可搜尋 PDF 應以「另存副本並加入隱形文字層」實作。

## 使用方式

將 PDF 拖曳到 `FamilyPDF-OCR.cmd`，會在 PDF 同一資料夾產生：

```text
原檔名.ocr.txt
```

也可在 PowerShell 或命令提示字元執行：

```powershell
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf"
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf" "D:\文件\結果.txt" -Languages chi_sim+eng
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf" "D:\文件\結果.txt" -Pages 1-3,8,10-12
```

常用參數：

- `-Languages chi_tra+eng`：繁體中文與英文，預設值。
- `-Languages chi_sim+eng`：簡體中文與英文。
- `-Pages 1-10`：只辨識第 1 到 10 頁。
- `-Dpi 300`：渲染解析度，允許 72–600 DPI。
- `-KeepPageImages`：保留 OCR 使用的逐頁 PNG，方便檢查辨識問題。

## 建置與封裝

執行：

```powershell
.\scripts\phase0\package-windows-runtime.ps1
```

腳本會在缺少時自動：

1. 下載 `eng`、`chi_tra`、`chi_sim` 語言資料。
2. 透過 vcpkg 安裝 Tesseract 與必要 DLL。
3. 將 OCR 執行檔、DLL、語言資料及啟動腳本加入 Windows ZIP。

若只想快速封裝不含 OCR 的測試版本，可使用 `-SkipOcr`。
