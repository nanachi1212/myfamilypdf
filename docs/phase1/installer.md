# Windows 安裝程式

FamilyPDF 提供兩個免管理員權限、支援繁體中文／簡體中文／英文的 x64 安裝程式：

- `dist\FamilyPDF-Setup-x64.exe`：Viewer、Editor、PageMaster、PdfTool 與必要 runtime。
- `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe`：選用 Tesseract OCR 外掛。

基礎安裝程式不包含 OCR，因此不使用 OCR 的家人不必下載額外語言模型。

## 建置

```powershell
.\scripts\phase0\build-installer.ps1
.\scripts\ocr\build-ocr-installer.ps1
```

若套件資料夾已經產生：

```powershell
.\scripts\phase0\build-installer.ps1 -SkipPackage
.\scripts\ocr\build-ocr-installer.ps1 -SkipPackage
```

腳本在缺少時會自動準備建置工具。Inno Setup 安裝檔在使用前會驗證 Windows Authenticode 簽章。

## 安裝位置與內容

預設安裝到：

```text
%LOCALAPPDATA%\Programs\FamilyPDF
```

基礎程式建立 Reader、Editor、Page Master 捷徑並提供正常解除安裝。OCR 外掛使用獨立 AppId 與解除安裝資料，不會覆蓋基礎程式的解除安裝項目。

Codex 沙盒無權建立使用者開始功能表與登錄；因此另有只供自動測試的 `-VerificationBuild`，停用這兩項後驗證完全相同的 payload。正式安裝器不會停用它們。
