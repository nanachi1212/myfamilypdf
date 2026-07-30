# FamilyPDF OCR 外掛

OCR 已與 FamilyPDF 基礎程式分開封裝。未使用 OCR 的家人只需安裝較小的基礎程式；需要掃描辨識時再安裝外掛。

## 已完成

- Tesseract 5.5.2。
- 橫排繁體中文 `chi_tra`、簡體中文 `chi_sim`、英文 `eng`。
- 自動補下載直排繁體 `chi_tra_vert`、直排簡體 `chi_sim_vert`；目前外掛已內建這兩個模型。
- 執行 OCR 時若選用的語言缺少，會直接啟動官方下載、重試、檔案檢查與 Tesseract 載入驗證，不必先手動修復。
- 將掃描 PDF 轉成新的可搜尋、可複製文字 PDF。
- 保留相同頁數；原始 PDF 永遠不會被覆寫。
- 可另外輸出 UTF-8 文字檔。
- 可指定 `1-3,8,10-12` 等頁面範圍、72–600 DPI 與 Tesseract 分頁模式。
- Viewer／Editor 的「工具」選單會在外掛安裝後直接啟動 OCR。

## 安裝

最省步驟的方式是直接執行 `FamilyPDF-Full-Setup-x64.exe`，預設會一次安裝
主程式與 OCR。若已使用基礎版，則再安裝：

```text
FamilyPDF-OCR-Plugin-Setup-x64.exe
```

可攜式版本則把 `FamilyPDF-OCR-Plugin-windows-x64.zip` 的內容解壓到 FamilyPDF 主程式資料夾。

外掛若發現語言資料缺失，可執行：

```text
Install-FamilyPDF-OCR-Languages.cmd
```

它只會從 Tesseract 官方 `tessdata_fast` 儲存庫下載缺少的五個語言檔。

## 使用

最簡單的方式是在 Viewer／Editor 開啟 PDF，選擇「工具 → 使用 OCR 建立可搜尋 PDF...」。

也可把 PDF 拖曳到 `FamilyPDF-OCR.cmd`，預設在原檔旁建立：

```text
原檔名.ocr.pdf
```

命令列範例：

```powershell
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf"
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf" "D:\文件\可搜尋版本.pdf"
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf" "D:\文件\可搜尋版本.pdf" -Languages chi_sim+eng
.\FamilyPDF-OCR.cmd "D:\文件\掃描檔.pdf" "D:\文件\部分頁面.pdf" -Pages 1-3,8,10-12
```

進階參數：

- `-OutputText "結果.txt"`：另外輸出 UTF-8 純文字。
- `-Languages chi_tra+chi_sim+eng`：橫排繁簡中文與英文，預設值。
- `-Languages chi_tra_vert+eng`：繁體中文直排與英文。
- `-Dpi 300`：渲染解析度。
- `-KeepPageImages`：保留辨識用 PNG 供人工檢查。

## 建置

```powershell
.\scripts\ocr\build-ocr-plugin.ps1
.\scripts\ocr\build-ocr-installer.ps1 -SkipPackage
```

缺少 Tesseract 相依套件時會透過 vcpkg 安裝；缺少語言檔時會自動從官方來源下載。

建置會固定執行橫排繁中、簡中、英文的實際辨識與可搜尋 PDF 回歸。當 `chi_tra_vert`、`chi_sim_vert` 都存在時，也會自動執行直排繁簡中回歸：

```powershell
.\scripts\ocr\Test-FamilyPDF-OCR-Horizontal.ps1
.\scripts\ocr\Test-FamilyPDF-OCR-Vertical.ps1
```
