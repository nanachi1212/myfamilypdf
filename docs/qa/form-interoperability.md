# AcroForm 互通性驗證

更新日期：2026-07-30

## 已完成的自動驗證

- `UnitTestsForms` 建立一頁 PDF，加入繁體中文文字欄位「姓名」及核取方塊「同意」。
- 文字欄位包含繁體中文預設值「繁體測試」、提示文字、必填、多行及最大 120 字。
- 核取方塊包含 `/V=/Yes`、`/DV=/Yes`、`/AP` 外觀串流及 Widget annotation。
- 測試將 PDF 寫入磁碟，再由 PDF4QT reader 與 `PDFForm::parse` 重讀；欄位型別、名稱、值、提示、旗標、頁面及核取外觀均一致。
- 產生的 `dist/qa/form-interop.pdf` 另以 `pypdf 6.14.2` 重讀，確認一頁、兩個欄位、Catalog 含 `/AcroForm`，文字欄位為 `/Tx`、核取方塊為 `/Btn`，且核取 Widget 含 `/AP`。
- `pypdf` 取得的中文 Unicode code points 分別為 `59D3 540D`、`7E41 9AD4 6E2C 8A66`、`540C 610F`；終端若缺中文字型可能顯示方框或替代字元，但實際 Unicode 值正確。
- `UnitTestsForms` 另建立含兩個 export value 的單選按鈕群組、含繁體／簡體顯示文字的下拉選單，以及可多選的清單；寫入磁碟後重讀並驗證 `/Btn`、`/Ch`、`/Opt`、`/V`、`/I`、欄位旗標與各單選 Widget 的 `/AP` 外觀均保留。
- 以最終繁中可攜包 GUI 開啟 `form-interop.pdf`，文字欄位的繁體中文內容與已勾選核取方塊均正常顯示；`Forms` 工具列亦由舊空白插件設定自動啟用。
- 以已安裝的 Adobe Acrobat DC，透過官方 Acrobat Interapplication Communication（IAC）介面開啟 `form-interop.pdf`、將文字欄位改為 `AdobeInterop2026`、取消核取並另存；再由 `pypdf` 獨立重讀，確認「姓名」「同意」兩個中文欄位名稱、修改值及原始預設值均保留。
- Adobe 互通測試曾找出 UTF-16BE 欄位名稱內的 `0x0D` 被 PDF literal string 換行正規化，導致「姓名」變成「姓吊」；writer 現在遇到 CR／LF 會改用 hexadecimal string，並以序列化測試固定 `<feff59d3540d>`。

執行 Adobe 互通回歸：

```powershell
.\scripts\qa\smoke-acrobat-form-interop.ps1
```

結果會寫入 `build\acrobat-form-interop\summary.json`。腳本只在啟動前沒有 Acrobat 行程時執行，避免干擾使用者正在編輯的文件。

## 尚待發佈前人工巡覽

- 在 FamilyPDF Editor 實際拖曳建立五種欄位、儲存、關閉、重開並逐一填值；目前 GUI 已驗證既有文字框與核取方塊顯示。
- 以 Tab 鍵確認欄位依頁面 annotation 建立順序移動。

目前已證明 PDF4QT、Adobe Acrobat DC 與 `pypdf` 的實際讀取／修改／另存互通；人工巡覽完成前，不宣稱五種欄位的所有 GUI 建立流程與鍵盤操作均已驗收。
