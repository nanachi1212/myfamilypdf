# FamilyPDF v0.2.1 Beta

這是 v0.2.0 Beta 的 OCR 安全性與可靠性修正版，適用於 Windows x64。

## 修正內容

- 固定語言模式的 JSON 報告現在會列出每個已處理頁面的語言與 PSM，不再輸出空的 `pages`。
- JSON 報告不再保存輸入或輸出檔名，降低分享報告時洩露敏感文件名稱的風險。
- 五個 Tesseract 模型固定至不可變 commit，下載前後都驗證精確 bytes 與 SHA-256。
- 模型檔即使存在，只要大小或 SHA-256 不符便會自動修復。
- 錯誤下載會被拒絕，且不留下已安裝模型或 `.download` 半檔。
- 所有輸出若指向既有目錄，會在渲染前拒絕，不產生部分 PDF。
- 新增 `-Pages`、`-KeepPageImages`、損壞模型修復與固定模式報告回歸。
- 新增 Windows GitHub validation workflow，檢查 PowerShell 5.1 語法、模型 pin 與下載雜湊保護。

## 驗證與限制

- Auto OCR 的繁體、簡體、混合、空白頁與部分頁面流程通過。
- 固定／自訂語言報告、模型損壞修復及下載竄改拒絕通過。
- 安裝程式仍未數位簽章，因此維持 Beta／Pre-release。
- OCR 結果，尤其法律文件、姓名、地址、案號及印章附近文字，仍應人工核對。
- GitHub workflow 目前負責輕量語法與 OCR metadata gate；完整 Windows 安裝包仍由本機驗證後手動發布。
