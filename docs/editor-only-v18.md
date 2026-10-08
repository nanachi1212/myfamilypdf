# 只出貨 Editor v18

更新日期：2026-10-08。可攜包與安裝程式只放 `Pdf4QtEditor.exe`（主程式）、`Pdf4QtDiff.exe`（文件比較）與 `PdfTool.exe`（命令列）。`Pdf4QtViewer.exe`、`Pdf4QtPageMaster.exe`、`Pdf4QtLaunchPad.exe` 仍會建置（CI 與測試使用），但不再打包。

## 為什麼

Editor 以 `AllFeatures` 初始化，是 Viewer 的超集合；PageMaster 的合併、拆分、插入、刪除、旋轉、重排已在 Editor（v8–v14）。少裝兩個入口，使用者不必判斷「該開哪一個」。

## 從 PageMaster 補進 Editor 的功能

- 縮圖右鍵 → **Select**：全部頁面、單數頁、雙數頁、反向選取、頁碼範圍（`1-3,8,10-12`）。選好後可直接刪除、旋轉、提取、列印、匯出。Viewer 的縮圖選單也有。
- File → Split Document... 新增 **在每個最上層書籤開始新檔案**（沒有可用書籤時選項反灰）。

沒搬的 PageMaster 功能（家用價值低、複雜度高）：工作區／檢查點存檔、群組與重組（Regroup）、依檔案大小拆分、依方向選取直式／橫式頁。

## 安裝程式變更

- 開始功能表：一個「FamilyPDF 編輯器」，移除閱讀器與頁面合併拆分捷徑；桌面捷徑與安裝完成後啟動都是 Editor。
- 右鍵「使用 FamilyPDF 開啟」與「開啟方式」改指向 Editor；移除重複的「使用 FamilyPDF 編輯」。登錄項從 12 筆減為 6 筆（`verify-installer-shell-integration.ps1` 同步更新）。
- 升級：舊版安裝過的 Viewer／PageMaster 檔案會被 Inno Setup 的覆蓋安裝保留在安裝資料夾中不再更新；解除安裝會一併刪除整個資料夾。

## 驗證

- 本機 Release：`pageSelectionHelpers`、`splitDocumentWorkflow` PASS。
- `verify-installer-shell-integration.ps1`（靜態檢查兩份 .iss）PASS；全部 PowerShell 腳本 parser 無錯誤。
- NOT_TESTED：以 Editor 重跑的安裝／升級／右鍵選單 smoke（`smoke-shell-installation.ps1`、`smoke-full-installer.ps1` 等需要隔離安裝，未執行）；`smoke-large-pdf-locales.ps1` 內的設定檔名仍是 Viewer 的，需在實測時確認。
