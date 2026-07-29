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
