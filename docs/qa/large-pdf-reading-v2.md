# 大型 PDF 閱讀體驗 v2 驗證

本輪基底：`fc06a2632015a8e6df3fb6a52467445269da16a5`（PR #8）。保留原 JPEG2000 色彩轉換加速、正常頁面畫質和既有 cache。

## 實作與量測

縮圖原本已直接 rasterize 到 target size，沒有先輸出高解析度整頁再 resize。編譯階段仍會 decode 原始圖片，因為編譯結果與正常頁面共用。100 px 縮圖的 rasterize/downsample 遠小於 JPEG2000 decode，因此本輪修改既有編譯器的待辦分派：可見頁優先、背景每批一頁，每批後重新讀取優先順序；已經執行中的一頁不強制中斷。沒有另建 scheduler/cache，沒有減少正常頁面的解析度。

同機器、相同 Qt benchmark 程式，比較保存的 baseline runtime 與主要排程修正 runtime。下表是單次樣本，適合確認排隊瓶頸，並非跨機器效能保證。

| 動作 | 真實 321 頁 before / after (ms) | 合成 364 頁 JPX before / after (ms) |
|---|---:|---:|
| 文件讀取完成 | 788 / 569 | 202 / 91 |
| 首頁編譯可用，含開啟 | 914 / 691 | 1500 / 1100 |
| 首張測試縮圖可用，含開啟 | 920 / 699 | 1511 / 1111 |
| 背景縮圖排入後跳第 20 頁 | 442 / 140 | 1209 / 968 |
| 第 100 頁 | 172 / 94 | 1371 / 1052 |
| 第 200 頁 | 182 / 132 | 1389 / 1100 |
| 末頁（321 / 364） | 132 / 214 | 1411 / 1088 |
| 冷 cache 快速跳 100、200、末頁 | 293 / 187 | 1354 / 1147 |
| 返回已渲染首頁 | 0 / 0 | 0 / 0 |

真實文件共 321 頁，使用 JPEG/Flate，沒有 JPX；第 364 頁由合成測試覆蓋。合成文件將一張固定亂數種子 123 的 1600 x 2200 lossless JPX 模擬文字掃描圖配置到 364 頁。不得把這份測試資料當成先前不同電腦的實際 364 頁 PDF。

JPX decode 約 440–500 ms，色彩轉換約 2–14 ms；73 x 100 px 縮圖 rasterize/downsample 約 6–12 ms。後者沒有可確認的改善；真實文件末頁單次跳頁也沒有改善。讀取時間容易受檔案系統 cache 影響，不能將所有首屏差異歸因於排程。

原有 compiled-page cache 預設 512 MiB，thumbnail pixmap cache 預設 64 MiB；compiled pages 每 5 秒檢查、非 active 頁面閒置 30 秒後可淘汰，另有容量淘汰。返回頁面實測 cache 命中，本輪沒有修改容量或淘汰策略。

## correctness 與 UI

- 重用 `PDFClosedIntervalSet`，增加提取專用的明確頁碼與範圍驗證。拒絕 0、負數、開放範圍、反向範圍、越界、溢位、非法文字、空項目；去除重複頁碼並依文件順序輸出。原有其他功能的 parser 語意不變。
- 提取使用 `QSaveFile`，不啟用 direct-write fallback；只有完整輸出才 commit。取消輸入或另存不寫檔，取消取代不破壞既有檔案。
- Viewer Qt 測試實際提取 `1-3,8,10-12`，輸出 7 頁後再由 FamilyPDF 開啟，真實與 JPX 文件均通過。core round-trip 另以各頁不同寬度確認順序。
- 正常頁面等待時顯示載入提示，完成後由原有 repaint 消失；編譯失敗才顯示錯誤，合法空白頁保留空白。縮圖也提供等待／錯誤文字。
- 補上新增訊息的繁中翻譯，修正五個 OCR 訊息的 `pdfviewer::PDFProgramController` context；沒有全面翻譯 upstream。
- 真實文件第 1、20、100、200、321 頁的 1000 px PdfTool before/after 渲染逐像素相同。Viewer 的第 20、100、200、末頁 normal-draw SHA-256 亦一致，合成 JPX 文件相同。

## 驗證範圍與重跑

本機 Release build 通過。`UnitTestsDocumentEdit` 共 23 項通過，涵蓋新的輸入資料列、輸出順序、原子取消及相關既有文件編輯測試。

完成上述主要排程量測後，補上可見頁先於首屏預載的 queue 請求、錯誤與翻譯檢查及 Editor smoke。最新 `UnitTestsViewer.exe` 與 `Pdf4QtLibGui.dll` 在本機被 Windows Code Integrity 簽章政策阻擋（3077 / 3033，DLL 錯誤 0xc0e90002）。最後小修沒有重新宣稱完整本機 Viewer/Editor 通過；最新程式的 runtime 驗證交由既有 Windows CI 執行，不停用安全功能。表格數據對應主要排程修正的已成功實測版本。

手動大型文件量測使用既有 Qt 測試，只有明確提供文件時才執行：

```powershell
$env:FAMILYPDF_LARGE_PDF = 'C:\path\large.pdf'
$env:FAMILYPDF_RENDER_PROFILE = '1'
.\build\phase0-upstream-release\usr\bin\UnitTestsViewer.exe largePdfReadingBenchmark
```

測試會使用隔離設定與暫存輸出，包含提取後重開。一般 CI 沒有提供大型文件時跳過 benchmark，照常執行相關 UI 與 Editor smoke。測試文件及實際閱讀畫面只留在本機 `build/reading-v2`，不提交或上傳真實 PDF。

正常頁面畫質檢查使用完整 CLI 參數，避免沒有文件參數的 Usage 視窗：

```powershell
.\build\phase0-upstream-release\usr\bin\PdfTool.exe render 'C:\path\large.pdf' --page-select '1,20,100,200' --image-output-dir 'build\pixels' --image-res-mode pixel --image-res-pixel 1000 --image-format png
```

## 本機 baseline runtime 診斷

Viewer、Core、Gui、Widgets 和所有測試 runtime DLL 的 PE 架構均為 x64（8664）；CMake 為 Release、MSVC 14.44.35207，Qt 為 6.9.1 msvc2022_64。先前 build-only 目錄沒有部署 Qt runtime，測試使用程序內 Qt 路徑。已依既有 Prepare-TestRuntime allowlist 補齊兩份隔離測試目錄的 Qt 6.9.1 release DLL、plugins 與 offscreen plugin，未修改系統 PATH。相同名稱的 Qt/plugin 和 vcpkg DLL 在兩份目錄中 SHA-256 均一致；PE 直接依賴核對沒有缺少檔案或架構衝突。System32 MSVC runtime 為 14.51.36247，UCRT 為 10.0.26100.9444。

完整 deployment 後，保存的 baseline runtime 重新執行 Viewer menu test，3 passed、0 failed；最新 UnitTestsViewer 仍被應用程式控制政策拒絕啟動。Code Integrity 3077 / 3033 明確記錄 Viewer 載入 Pdf4QtLibGui.dll 不符合 Enterprise signing level，policy ID 為 0283ac0f-fff1-49ae-ada1-8a933130cad6。這是本機政策阻擋，沒有證據指向二進位相容性 regression；未停用政策、重裝系統元件或混用舊版產品 DLL。診斷報告與完整雜湊留在本機 build/reading-v2/runtime-comparison.json。

PdfTool 畫質量測使用上方完整 render 參數；診斷後確認沒有遺留 PdfTool、Viewer 或 Editor 程序。
