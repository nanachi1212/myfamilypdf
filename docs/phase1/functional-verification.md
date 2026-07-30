# Phase 1 功能驗證結果

更新日期：2026-07-29

## 非 OCR 完成版回歸（2026-07-29）

- 四組 CTest 測試全部通過，包括 16 個書籤、安全儲存、復原、工作階段、標準 outline 與註解互通測試。
- 書籤支援文字顏色、資料夾、階層、收合／展開及舊格式自動升級。
- 標準 PDF outline 新增「Insert Folder」及「Text Color...」操作；繁體中文標題、階層、顏色、粗體與頁面目的地已由 pypdf 獨立讀回。
- Highlight、Underline、StrikeOut、Square、FreeText、Text 六種標準註解已由 FamilyPDF 嚴格模式重開，並由 pypdf 讀回 subtype、顏色及文字。
- Viewer／Editor 已改為同一程式內的多文件分頁；重開程式可還原上次文件工作階段。
- 安全儲存會偵測外部修改、驗證暫存候選檔、使用 Windows 原子取代，並保留最多三份隱藏備份。
- 1,160 頁測試 PDF 已由正式安裝 payload 的 Viewer／Editor 載入 15 秒，兩者皆為 Responding；工作集約 100.7 MB／146.9 MB。
- 單數頁、雙數頁及 `10-20` 範圍回歸分別得到 29、29、11 頁。
- 繁體／簡體翻譯檔已重新產生；四種 Viewer／Editor 語系啟動組合皆維持 Responding。
- 正式安裝 payload 透過隔離驗證安裝器靜默安裝成功（exit code `0`）。正式安裝器仍保留開始功能表捷徑及解除安裝登錄。
- 基礎 ZIP 與安裝程式不含 Tesseract、OCR 語言資料、測試執行檔或 Qt 除錯 DLL。

以下較早的 OCR 內嵌封裝紀錄只保留作歷史比較；目前產品已改為「基礎程式與 OCR 外掛分開安裝」。

## OCR 外掛完成版回歸（2026-07-29）

- OCR 外掛已更新為 0.3.0；驗證安裝 exit code `0`，manifest 與安裝 payload 版本一致。
- 單頁及雙頁 PDF 均成功建立同頁數的可搜尋 PDF，`PdfTool fetch-text` 可抽取 `FamilyPDF` 文字。
- 測試同時確認原始 PDF SHA-256 未改變，且可選擇另外輸出非空白 UTF-8 文字檔。
- 固定橫排圖像回歸直接以 `chi_tra`、`chi_sim`、`eng` 辨識，分別取得「傳統中文測試」、「简体中文测试」及 `FamilyPDF OCR 2026`。
- 封裝後端對端流程確認原始 PDF SHA-256 不變、頁數不變、PDF 文字層含預期繁簡中文字元，UTF-8 sidecar 則保留完整字序。
- 最新重建的 Inno Setup 7.0.2 安裝檔編譯成功；隔離靜默安裝 exit code `0`，安裝後 manifest、五個模型及橫排／直排回歸均通過。
- 語言修復腳本會自動補下載 `eng`、`chi_tra`、`chi_sim`、`chi_tra_vert`、`chi_sim_vert`；本次已成功補齊兩個直排模型並驗證 SHA-256 與 Tesseract 實際載入。
- OCR 主流程已直接串接缺語言修復：官方來源、最多三次重試、大小檢查、Tesseract 實際載入與原子替換。斷線回歸確認不產生結果 PDF，且沒有 `.download` 半檔。
- 橫排繁簡中回歸與直排模型回歸都已接入每次 OCR 封裝建置；直排測試直接驗證兩個模型的精確辨識，並驗證 FamilyPDF 產生的可搜尋 PDF 文字層與頁數／原檔不變。
- Viewer／Editor 已加入「使用 OCR 建立可搜尋 PDF」工具選單；未安裝外掛時會顯示明確提示。

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

- 繁體中文與簡體中文 Windows locale 的兩台實機長時間人工操作；目前已完成兩種中文安裝語系、翻譯 payload 與中文檔名自動回歸。
- 1,160 頁 PDF 的長時間人工快速翻頁體感；目前已完成 15 秒 Viewer／Editor Responding smoke test。
- 直排繁／簡中文模型因目前 Codex 沙盒阻擋下載，尚未在本機完成實際直排頁面辨識回歸；自動下載與缺檔檢查已實作。
