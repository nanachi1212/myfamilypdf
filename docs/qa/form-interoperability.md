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

## 尚待發佈前人工巡覽

- 在 FamilyPDF Editor 實際拖曳建立五種欄位、儲存、關閉、重開並逐一填值；目前 GUI 已驗證既有文字框與核取方塊顯示。
- 使用另一套 GUI 閱讀器（建議 Microsoft Edge 或 Adobe Acrobat Reader）開啟 `form-interop.pdf`，確認文字框可輸入、核取狀態可切換並在儲存後保留。
- 以 Tab 鍵確認欄位依頁面 annotation 建立順序移動。

人工巡覽完成前，本文件只證明 PDF 結構及兩套獨立 parser 的互通性，不宣稱已完成所有 GUI 閱讀器驗收。
