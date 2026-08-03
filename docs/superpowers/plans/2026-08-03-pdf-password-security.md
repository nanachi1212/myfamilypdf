# PDF Password Security Implementation Plan

> **Status:** 已完成本機實作與驗證；GitHub 推送待 TCP 443 恢復。

**Goal:** 將 FamilyPDF 既有的 PDF 密碼保護、AES-256、擁有者權限與解密能力納入正式家庭版交付及自動回歸。

**Architecture:** 使用正式可攜包／安裝包內的 `PdfTool.exe` 執行原地加密與解密，但 QA 永遠先複製 fixture，避免修改來源。以 `pypdf` 獨立驗證 encryption dictionary、錯誤／正確密碼、文字與頁數；再確認 Editor 開啟加密檔時出現可回應的密碼流程。

**Tech Stack:** PDF4QT、AES-256、PowerShell 5.1、pypdf、pypdfium2、CTest、Inno Setup。

---

## File structure

- Create: `scripts/qa/smoke-pdf-security.ps1` — 建立 fixture，驗證 AES-256、密碼、權限、解密及 GUI。
- Modify: `scripts/qa/run-final-regression.ps1` — 將 PDF security 納入正式可攜版總回歸與 summary。
- Modify: `scripts/qa/smoke-full-installer.ps1` — 驗證完整與精簡安裝後的安全功能。
- Modify: `README.md`、`docs/REQUIREMENTS-AUDIT.md`、`docs/RELEASE-STATUS.md`、`docs/WORKSPACE-HANDOFF.md` — 加入密碼保護功能與證據。

### Task 1: 建立 PDF security 冒煙測試

**Files:**
- Create: `scripts/qa/smoke-pdf-security.ps1`

- [x] **Step 1: 建立固定兩頁 PDF**

使用 Office Export venv 與既有 `_write_two_page_pdf` 建立含 `First page`、`Second page` 的兩頁來源 PDF；複製成 encryption target，來源檔永不直接修改。

- [x] **Step 2: 以 AES-256 加密複本**

```powershell
& $pdfTool encrypt --enc-algorithm aes-256 --enc-contents all `
    --enc-user-password $userPassword `
    --enc-owner-password $ownerPassword `
    --enc-permissions 0 $encryptedPdf
```

Expected: exit code `0`；來源 SHA-256 不變，加密目標 SHA-256 改變。

- [x] **Step 3: 獨立驗證密碼與權限**

以 `pypdf` 驗證：`is_encrypted`、`/V = 5`、`/R = 6`、錯誤密碼回傳 `NOT_DECRYPTED`、使用者與擁有者密碼均可解鎖、頁數為 2、文字仍含 `First page`／`Second page`，且 `/P` 未授予列印、修改、複製及表單權限。

- [x] **Step 4: 驗證 Editor 密碼流程與安全解密**

啟動 `Pdf4QtEditor.exe encrypted.pdf`，等待密碼視窗／主程序可回應後安全結束；再複製 encrypted PDF，以 `PdfTool.exe decrypt --pswd <owner>` 解密複本，獨立確認 `is_encrypted = false`、頁數與文字均保留。

- [x] **Step 5: 輸出 JSON summary 並執行單一 smoke**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-pdf-security.ps1
```

Expected: exit code `0`；summary 包含 `algorithm: AES-256`、`pages: 2`、`wrong_password_rejected: true`、`permissions_restricted: true`、`decrypted_text_preserved: true`、`gui_responding: true`。

### Task 2: 納入可攜版與安裝版回歸

**Files:**
- Modify: `scripts/qa/run-final-regression.ps1`
- Modify: `scripts/qa/smoke-full-installer.ps1`

- [x] **Step 1: 整合最終回歸**

在 `run-final-regression.ps1` 對最新 portable staging 呼叫 security smoke，並把六個 security 欄位寫入最終 `summary.json`。

- [x] **Step 2: 整合完整／精簡安裝回歸**

對 full 與 core-only 安裝目錄各執行 security smoke；summary 記錄兩種模式 `pdf_security: true`。不得把任何測試密碼或 fixture 放進正式安裝包。

- [x] **Step 3: 執行完整回歸與隔離安裝**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\run-final-regression.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\qa\smoke-full-installer.ps1
```

Expected: CTest 6/6、Office 11/11、PDF security、OCR、PDF 比較、多文件及完整／精簡安裝全部通過。

### Task 3: 文件、驗證與提交

**Files:**
- Modify: `README.md`
- Modify: `docs/REQUIREMENTS-AUDIT.md`
- Modify: `docs/RELEASE-STATUS.md`
- Modify: `docs/WORKSPACE-HANDOFF.md`

- [x] **Step 1: 更新家庭版功能與驗證邊界**

記錄 Editor 可設定 AES-256、使用者／擁有者密碼、內容範圍及權限；說明忘記擁有者密碼無法繞過，DRM／憑證互通仍需實際文件驗證。

- [x] **Step 2: 提交前重新驗證**

重新執行 security smoke、PowerShell parser、`git diff --check`，並確認三個正式產物 bytes／SHA-256 未因純 QA／文件變更而改變。

- [x] **Step 3: 提交並檢查 GitHub 網路**

```powershell
git add scripts README.md docs
git commit -m "test: verify PDF password security delivery"
git push origin codex/phase0-baseline
```

Expected: 本機提交成功；只有在 TCP 443 恢復時才推送，否則保留乾淨分支並回報 ahead 數。
