# FamilyPDF v0.2.0 Beta

這是第一個公開的 FamilyPDF 預發佈版本，適用於 Windows x64。一般使用建議下載 `FamilyPDF-Full-Setup-x64.exe`，一次安裝主程式與 OCR。

## 主要更新

- OCR 預設採用 `Auto`：逐頁判斷繁體、簡體或繁簡混合內容，必要時比較不同版面模式。
- 自動產生 `.ocr-report.json`，標示每頁語言、信心及需要人工複核的頁面。
- 保留 `Traditional`、`Simplified`、`English` 與自訂語言模式，適合已知語言或追求速度的文件。
- 新增來源與所有輸出路徑互斥保護，結果不會覆寫來源 PDF。
- 主程式與 OCR 可攜包均補入第三方授權資訊。

## 下載選擇

- `FamilyPDF-Full-Setup-x64.exe`：建議，一次安裝主程式與 OCR。
- `FamilyPDF-Setup-x64.exe`：只安裝主程式，不含 OCR。
- `FamilyPDF-OCR-Plugin-Setup-x64.exe`：已安裝主程式時單獨加入 OCR。
- 兩個 `windows-x64.zip`：免安裝使用；需要 OCR 時把 OCR ZIP 覆蓋到主程式資料夾。
- `SHA256SUMS.txt`：下載後完整性核對。

## 本版驗證

- CTest 6/6、Office Export 11/11。
- 自動 OCR：繁體、簡體、混合與空白頁均通過；橫排與直排模型回歸通過。
- 完整版與純核心版隔離安裝、逐檔 SHA-256、Viewer 啟動與 PDF 安全功能通過。
- OCR 外掛隔離安裝及解除安裝通過，47 個封裝檔案逐檔相同。
- OCR 與 Core 共用目錄時，解除 OCR 後 425 個 Core 檔案雜湊不變，Viewer 仍可啟動。
- Windows PDF 右鍵選單／檔案關聯安裝及移除 round-trip 通過。

## 已知限制與建議

- 三個安裝程式尚未數位簽章，Windows 可能顯示「未知發行者」或 SmartScreen 警告；穩定版前應加入程式碼簽章與可信時間戳。
- `Auto` 會執行額外探測，測試資料上約比固定語言模式慢 2.8 倍；已知文件語言時可選 `Traditional` 或 `Simplified`。
- OCR 是啟發式辨識。法律文件、人名、地址、案號、印章附近文字與複雜表格仍應人工核對 JSON 報告所列低信心頁。
- JSON 報告不含完整路徑，但仍保留輸入與輸出檔名；分享報告前請確認檔名不含敏感資訊。
- 每頁分析報告目前以預設 `Auto` 模式為主；固定語言模式若另外指定 `OutputReport`，`pages` 會是空陣列。
- 目前只提供 Windows x64，尚未完成 Windows ARM64 與乾淨虛擬機／不同使用者帳號矩陣。
- OCR 語言修復功能的遠端模型來源尚未固定到不可變 commit 與雜湊；本次 release 內建模型不受影響，後續應固定來源並驗證 SHA-256。
- GitHub 內現有 workflows 仍是上游 PDF4QT 流程，無法可靠建立 FamilyPDF 產物。本版由本機完整驗證後手動發布；後續應建立 FamilyPDF 專用 CI/release workflow。
- 產品版本目前分為 FamilyPDF 0.2.0、OCR 外掛 0.4.0、上游 PDF4QT About 1.6.0.0。後續應由單一版本檔注入安裝器、應用程式 About 與 release tag。
- 套件已加入第三方 notices 與主要 vcpkg 授權檔，但尚未完成法律意見、完整 SBOM 與 Qt／FFmpeg 授權文字盤點；正式商業散布前應再做授權稽核。

基於以上限制，本版標示為 Beta／Prerelease；完成簽章、乾淨 Windows 驗收與專用 CI 後，再升為穩定版較妥當。
