# FamilyPDF v0.2.2 Beta

這是 Windows x64 預發佈版本。一般使用建議下載 `FamilyPDF-Full-Setup-x64.exe`，一次安裝主程式與 OCR 0.4.2。

## 本版修正

- OCR 的 PDF、TXT、JSON 與保留頁面圖片改為交易式發布：所有候選輸出先在暫存區完成；任一目的地被鎖定或發布失敗時，會回復原有輸出，不留下看似成功的半成品 PDF。
- 新增 `PdfTool`、Tesseract 非零退出與 sidecar 鎖檔故障注入測試，並持續驗證來源 PDF SHA-256 不變。
- `VERSION` 與 `OCR_VERSION` 成為 installer 與 OCR manifest 的唯一版本來源；GitHub validation 會阻止版本漂移。
- Base portable package 加入 Qt 6.9.1 官方 SPDX SBOM，包含 Qt Multimedia 與 FFmpeg 的套件關係；Office helper 加入 hash-locked dependency 清單與第三方 notices。

## 驗證結果

- CTest 6/6、Office Export 11/11、PDF security、Diff、shell integration、1,160 頁 PDF 與 GUI responsive 回歸通過。
- Full installer：完整安裝逐檔驗證 Core 431／OCR 48；Core-only 逐檔驗證 431。
- OCR installer：48 個檔案逐檔驗證，解除安裝後 432 個 Core 檔案保持不變，Viewer 可正常啟動。
- 兩個 portable ZIP 重新解壓後，Base 432／OCR 48 個檔案與來源目錄逐檔 SHA-256 相同。
- 以 8 頁真實法律文件重新驗收：來源 SHA-256 不變，文字與 OCR 0.4.1 完全相同；第 6、7、8 頁仍標為需人工複核，JSON 不含來源或輸出檔名。

## 已知限制

- 安裝程式尚未數位簽章，Windows 可能顯示未知發行者或 SmartScreen 警告。
- 目前只提供 Windows x64；Windows ARM64 與全新 Windows Sandbox／不同帳號矩陣尚未完成。本機查詢 Windows Sandbox 功能需系統管理員權限，本次未變更系統設定。
- GitHub validation 目前驗證 PowerShell 5.1 語法、版本一致性與 OCR 模型下載安全；完整 Windows build／installer smoke 仍由本機執行。
- 已附主要第三方 notices、vcpkg 授權與 Qt 官方 SPDX SBOM，但尚未取得法律意見，也尚未把 Qt／FFmpeg 所有授權全文與完整 Office SBOM 整合為單一法律審查包。
- Auto 模式較固定語言慢；法律文件、人名、地址、案號、印章附近文字與低信心頁面仍須人工核對。

因此本版維持 Beta／Prerelease；完成程式碼簽章、全新 Windows 驗收、ARM64 決策與授權法律複核後，再升為 stable 較妥當。
