# Windows 安裝程式

FamilyPDF 提供三個免管理員權限、支援繁體中文／簡體中文／英文的 x64 安裝程式：

- `dist\FamilyPDF-Full-Setup-x64.exe`：建議家人使用；預設一次安裝主程式與 OCR，安裝時也可取消 OCR。
- `dist\FamilyPDF-Setup-x64.exe`：Viewer、Editor、PageMaster、PdfTool 與必要 runtime。
- `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe`：選用 Tesseract OCR 外掛。

基礎安裝程式不包含 OCR，因此不使用 OCR 的家人不必下載額外語言模型；需要全部功能時只要執行完整安裝程式一次。

## 建置

```powershell
.\scripts\phase0\build-installer.ps1
.\scripts\ocr\build-ocr-installer.ps1
.\scripts\phase0\build-full-installer.ps1
```

若套件資料夾已經產生：

```powershell
.\scripts\phase0\build-installer.ps1 -SkipPackage
.\scripts\ocr\build-ocr-installer.ps1 -SkipPackage
.\scripts\phase0\build-full-installer.ps1 -SkipBasePackage -SkipOcrPackage
```

腳本在缺少時會自動準備建置工具。Inno Setup 安裝檔在使用前會驗證 Windows Authenticode 簽章。

## 安裝位置與內容

預設安裝到：

```text
%LOCALAPPDATA%\Programs\FamilyPDF
```

基礎程式建立 Reader、Editor、Page Master 捷徑並提供正常解除安裝。OCR 外掛使用獨立 AppId 與解除安裝資料，不會覆蓋基礎程式的解除安裝項目。

Codex 沙盒無權建立使用者開始功能表與登錄；因此另有只供自動測試的 `-VerificationBuild`，停用這兩項後驗證完全相同的 payload。正式安裝器不會停用它們。

## Windows PDF 整合

基礎版與完整版都預設勾選使用者層級的 Windows 整合：

- PDF 右鍵選單加入「使用 FamilyPDF 開啟」與「使用 FamilyPDF 編輯」。
- Viewer 與 Editor 會出現在 Windows「開啟方式」。
- 只寫入 `HKCU\Software\Classes`，不需要管理員權限。
- 不直接修改 `.pdf` 的預設 ProgID，因此不會搶走 Edge、Adobe 或其他既有預設閱讀器。
- 取消安裝時只刪除 FamilyPDF 自己建立的四個 Registry 子樹。

安裝時可以取消這個選項。來源規則可用下列命令重跑檢查：

```powershell
.\scripts\qa\verify-installer-shell-integration.ps1
```

## 完整安裝程式驗證

```powershell
.\scripts\qa\smoke-full-installer.ps1
```

測試會在 `build\full-installer-smoke` 建立兩個隔離安裝：完整模式必須包含
主程式、五個 OCR 模型並完成繁簡中可搜尋 PDF 回歸；精簡模式必須能啟動
Viewer，且不得包含 OCR 檔案。
