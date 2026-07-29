# Phase 1 功能驗證結果

更新日期：2026-07-29

## 可攜式包 smoke test

- `Pdf4QtViewer.exe`：開啟 290 頁 PDF 後維持執行 8 秒，仍在執行且 Windows 回報 Responding。
- `UnitTests.exe`：exit code `0`。
- `UnitTestsImageOptimizer.exe`：exit code `0`。
- `UnitTestsFontEncoding.exe`：exit code `0`。
- 可攜式包包含 `translations/PDF4QT_zh_TW.qm` 與 `translations/PDF4QT_zh_CN.qm`。
- PageMaster 合併流程已改為：有選取頁面時只合併選取內容；無選取時合併全部內容。可先使用 `Select Even`、`Select Odd` 或 `Select Page Range...` 建立選取。
- 精簡封裝後不含 Qt 除錯 DLL，三組單元測試仍全部以 exit code `0` 通過。
- 最新含 OCR 可攜式 ZIP 為 96,063,098 bytes（約 91.6 MiB）；封裝腳本保留完整 release Qt fallback，避免 `windeployqt` 偶發失敗造成缺檔。
- 封裝版 Tesseract 5.5.2 成功載入 `chi_tra`、`chi_sim`、`eng`。
- 封裝版 `FamilyPDF-OCR.ps1` 成功辨識繁體中文、英文與測試 PDF 內的自由文字註解。

## 命令列 PDF 流程

使用可攜式包內的 `PdfTool.exe` 與基底測試 PDF：

- `help`：成功列出 `unite`、`separate`、`fetch-text` 等命令。
- `unite a.pdf b.pdf merged.pdf`：exit code `0`，成功產生合併檔。
- `info merged.pdf`：exit code `0`，合併檔為 58 頁。
- `separate merged.pdf split-%.pdf`：exit code `0`，成功產生 58 個分頁 PDF。
- 從靜默安裝後的 `PdfTool.exe` 再次執行上述流程：合併約 217 ms、58 頁資訊驗證成功、拆分產生 58 個檔案。

大型檔案基準：

- 以 5 份 58 頁 PDF 合併成 290 頁：exit code `0`，約 0.50 秒。
- `info` 讀取 290 頁 PDF：exit code `0`，約 0.35 秒，檔案大小 867,678 bytes。
- 用安裝後程式實際開啟 290 頁 PDF 並維持 8 秒：
  - `Pdf4QtViewer.exe`：仍正常執行且 Responding，工作集約 130.9 MB。
  - `Pdf4QtPageMaster.exe`：仍正常執行且 Responding，工作集約 90.3 MB。

## 安裝檔

- 已使用 Inno Setup 7.0.2 成功編譯 `dist\FamilyPDF-Setup-x64.exe`。
- 編譯時已載入繁體中文、簡體中文與英文安裝語系。
- Inno Setup 下載檔已先通過 Windows Authenticode 驗證，簽章有效且發行者為 `Pyrsys B.V.`。
- 最終安裝檔包含 OCR，大小 74,105,033 bytes（約 70.7 MiB）。
- SHA-256：`CB0F2A396F27F63948009B607DDBD9628BA730D1844527795A1543213CCAC6DA`。
- 使用 `/VERYSILENT` 安裝到隔離目錄：exit code `0`。
- 從安裝後目錄執行三組單元測試：全部 exit code `0`。
- 安裝後 Tesseract 語言列舉與完整 OCR 腳本：exit code `0`。

驗證素材位於本機 `build/phase1-verification/`，未納入 Git；可重新產生，不影響原始 PDF。

## GUI 註解與書籤

- 已在 Editor 建立自由文字註解 `FamilyPDF 測試註解`，儲存 PDF 後關閉並重新開啟，註解仍存在。
- 已用 `Ctrl+M` 建立第一頁使用者書籤，關閉 Editor、重新啟動並開啟同一份 PDF 後，再按一次 `Ctrl+M` 可移除既有書籤，證明書籤已從本機設定載入。
- 再按一次 `Ctrl+M` 後，書籤 JSON 恢復為一筆第一頁書籤。
- 書籤以正規化後的 PDF 絕對路徑 SHA-256 作為索引；實際檔案路徑不寫入 JSON。
- Viewer 與 Editor 共用 `%LOCALAPPDATA%\FamilyPDF\bookmarks.json`。
- 最終安裝版回歸以獨立 PDF 驗證 JSON 計數 `1 → 0 → 1`：重新啟動後載入既有書籤、移除、再重新啟動建立，兩次 Editor 都正常關閉。
- 跨程式回歸：Editor 建立的書籤由 Viewer 載入並移除，再由 Editor 載入空狀態並重建；計數同樣為 `1 → 0 → 1`。

## 基礎閱讀操作與多檔

- Viewer／Editor 均有放大、縮小、縮放百分比、符合整頁、符合頁寬及符合頁高。
- 左側縮略圖面板支援點擊跳頁、縮圖大小調整及與目前頁面同步。
- 工具列頁碼欄支援直接輸入頁碼，並顯示總頁數；另有上一頁、下一頁、文件開頭／結尾。
- 同時啟動三個安裝版 Viewer，分別開啟兩份一般 PDF 與一份 290 頁 PDF；10 秒後三個行程皆 Running、Responding，並正常關閉。
- 多檔目前採獨立視窗／獨立行程，不是單一視窗分頁。

## OCR 辨識

- Tesseract 版本：5.5.2；Leptonica 版本：1.87.0。
- 語言資料：繁體中文 `chi_tra`、簡體中文 `chi_sim`、英文 `eng`。
- 直接 Tesseract 測試與完整 `FamilyPDF-OCR.ps1` 流程皆成功。
- 完整流程使用更新後 `PdfTool.exe --render-hw-accel 0`，已確認不再出現舊版錯誤警告。
- 繁中測試輸出包含 `繁體中文測試` 與正確的 `FamilyPDF OCR Test 2026`；原 PDF 的 `FamilyPDF 測試註解` 也可被辨識。

## 尚未由自動測試證明的項目

- GUI 中每一種彩色文字標記工具的逐項保存回歸；自由文字註解已完成保存與重開驗證。
- 手動書籤目前沒有使用者自訂文字顏色或可收合資料夾；自動／手動書籤只以藍色／橘色星號區分。
- 繁體中文與簡體中文 Windows locale 的實機顯示。
- 數百頁大型 PDF 的實際人工快速翻頁體感與長時間壓力測試。
- OCR 第一版不建立可搜尋 PDF 隱形文字層，只輸出 UTF-8 文字檔。
