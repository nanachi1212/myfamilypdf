# Windows 安裝檔

FamilyPDF 使用 Inno Setup 產生目前使用者範圍的 x64 安裝檔，不需要系統管理員權限。安裝介面包含繁體中文、簡體中文與英文。

## 建置

```powershell
.\scripts\phase0\build-installer.ps1
```

腳本會自動：

1. 建立含 OCR 的最新可攜式套件。
2. 在缺少時下載固定版本的 Inno Setup 7；該版本內建繁體中文與簡體中文安裝語系。
3. 用 Windows Authenticode 驗證下載檔簽章及發行者。
4. 將編譯器安裝到 `E:\CodexProject\FamilyPDF-tools`，不加入全域 PATH。
5. 產生 `dist\FamilyPDF-Setup-x64.exe`。

開發時若可攜式包已是最新，可使用：

```powershell
.\scripts\phase0\build-installer.ps1 -SkipPackage
```

若只要不含 OCR 的快速測試安裝檔，可使用：

```powershell
.\scripts\phase0\build-installer.ps1 -SkipOcr
```

## 安裝內容

- FamilyPDF 閱讀器
- FamilyPDF 編輯器
- FamilyPDF 頁面合併與拆分
- FamilyPDF OCR
- 可選的桌面捷徑
- 開始功能表捷徑與解除安裝項目

## 已驗證產物

2026-07-29 的 `dist\FamilyPDF-Setup-x64.exe`：

- 大小：74,105,033 bytes（約 70.7 MiB）。
- SHA-256：`CB0F2A396F27F63948009B607DDBD9628BA730D1844527795A1543213CCAC6DA`。
- 使用 `/VERYSILENT` 安裝到隔離目錄，安裝程式 exit code `0`。
- 從安裝後目錄執行三組單元測試，全部 exit code `0`。
- 安裝後 Tesseract 5.5.2 可載入 `chi_tra`、`chi_sim`、`eng`。
- 安裝後 OCR 成功辨識繁體中文、英文與既有 PDF 文字註解。
- 安裝後 Viewer 與 PageMaster 成功開啟 290 頁 PDF 並保持回應。

目前 FamilyPDF 安裝檔沒有商業程式碼簽章憑證，Windows SmartScreen 可能顯示「未知發行者」。建置流程下載的 Inno Setup 本身則會先驗證有效 Authenticode 簽章及發行者。
