# FamilyPDF 0.2.3 覆蓋升級說明

## 結論

已安裝 FamilyPDF 0.2.2 的電腦不需要先解除安裝。

關閉所有 FamilyPDF 視窗後，直接執行：

`E:\CodexProject\FamilyPDF\dist\FamilyPDF-Full-Setup-x64.exe`

安裝程式會辨識原本的 FamilyPDF AppId，沿用舊版安裝目錄、開始功能表群組、語言及安裝工作選項，再將程式升級到 0.2.3。

0.2.3 會把程式內部設定名稱從 `PDF4QT ...` 統一為 `FamilyPDF ...`。這次升級以目前沒有自訂設定的裸安裝為目標，不搬移舊版偏好設定或工作階段。

## 升級前

1. 儲存並關閉所有 FamilyPDF 視窗。
2. 不要解除安裝 0.2.2；解除安裝不是升級的必要步驟。
3. 建議保留重要 PDF 的一般備份。升級流程不會刪除使用者 PDF、書籤或偏好設定，但重要資料仍應有獨立備份。

## 執行升級

1. 雙擊 `FamilyPDF-Full-Setup-x64.exe`。
2. Windows 若顯示未簽章程式警告，先確認檔案路徑及下方 SHA-256，再決定是否執行。
3. 依照安裝精靈完成安裝；不需要手動選回舊目錄。
4. 啟動 FamilyPDF，確認程式可正常開啟並建立新的 FamilyPDF 設定。

## 安裝包驗證

- 產品版本：`0.2.3`
- 檔案大小：`69,379,484 bytes`
- SHA-256：`42FC159AA070DCEEAFA3BEB3583C4918FA562F9E88E3C5F7AA57E30D3FD1E57A`

可用 PowerShell 自行核對：

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'E:\CodexProject\FamilyPDF\dist\FamilyPDF-Full-Setup-x64.exe'
```

## 實測範圍

升級測試使用獨立的測試 AppId 與暫存安裝目錄，實際完成 `0.2.2 -> 0.2.3` 覆蓋升級，確認：

- 沿用原安裝目錄。
- 登錄的顯示版本更新為 0.2.3。
- 升級前放入安裝目錄的測試檔案仍存在。
- 新版執行檔存在。
- 測試完成後，臨時安裝、登錄項目及暫存目錄已清除。

完成隔離測試後，也已依照使用者授權，使用相同安裝包將本機正式 FamilyPDF 從 0.2.2 覆蓋升級為 0.2.3。正式安裝目錄中的 Editor、Reader、PageMaster 與 Diff 雜湊均與最終套件一致。
