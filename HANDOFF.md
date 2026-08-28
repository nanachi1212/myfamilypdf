# FamilyPDF 接手說明書

> 給接手本資料夾的其他 AI：先讀本文件，再讀列出的規則與入口。本文是目前盤點摘要；開始工作前仍須重新確認 Git 狀態、檔案時間與可執行環境。

## 1. 這裡是什麼

家庭用 Windows x64 PDF 閱讀／編輯工具。

## 2. 目前已知入口與邊界

AGENTS.md、README.md、docs/；不要未經要求執行 build。先看 docs/RELEASE-STATUS.md、docs/WORKSPACE-HANDOFF.md 和目前 Git 狀態。

## 3. 接手順序

1. 確認目前工作目錄就是：E:\\CodexProject\\FamilyPDF。
2. 若存在 AGENTS.md、CLAUDE.md、README.md 或 README.MD，按順序讀完相關規則，再讀專案入口與測試。
3. 執行唯讀盤點：git status --short --branch、git remote -v、git log -1 --oneline；若不是 Git repository，明確記錄這一點。
4. 先提出「目標、影響檔案、驗證命令、風險」，未得到必要確認前不要 push、刪除、安裝或修改秘密。
5. 修改後實際執行適用的測試／lint／smoke check，並回報命令與結果；不要用「理論上可以」代替證據。

## 4. 安全規則

- 把 .env、token、API Key、帳戶資料、個人研究與使用者文件視為秘密；只能讀取必要部分，不得貼到回覆、commit 或遠端。
- 原始資料、歷史 log、EXE／APK／DLL 和輸出檔先保留；清理前先列清單並採可復原方式。
- 不要把本文件中的描述當成最新版本號、測試通過或 GitHub 同步證明；那些都要現場查證。
- 若發現資料夾其實是產物、快照、快取或外部依賴，停止直接修它，回到對應原始碼資料夾。

## 5. 完成回報格式

- 實際目標與範圍
- 修改檔案
- 實際執行的測試／驗證命令與結果
- Git 分支與未提交變更
- 尚未驗證或需要使用者決定的事項

## 6. 目前已完成狀態（2026-08-27）

- FamilyPDF 版本：`0.2.3`。
- 已完成核心 Bug、安全儲存、外部程序 timeout、路徑可攜性、品牌名稱與 Qt platform plugin 測試修正。
- 依優先級補強簽署 PDF／附件的 `QSaveFile` 原子輸出，以及 TTS proxy、語音引擎、播放文件與同步控制項的 Release null guard；相關靜態契約已納入 GitHub validation workflow。
- `UnitTestsBookmarks` 已固定使用 Qt `offscreen` 平台並部署對應 plugin，修正測試啟動時可能出現的 platform plugin 初始化錯誤。
- TTS 的 `updateUI()`、`setSettings()` 已補上 UI 初始化與 null 設定防護，避免 public lifecycle 呼叫順序造成 Release crash。
- `UnitTests/CMakeLists.txt` 已將重複的測試 target 設定集中到共用函式；各測試 target 與 CTest 行為維持不變。
- Office Export 的 DOCX／XLSX writer 已改用同目錄暫存檔與原子替換，Python 測試由 11/11 增至 13/13，並納入 GitHub validation。
- recovery snapshot 與 `PDFSafeSaveService` 的非 Windows 覆蓋提交已改用 POSIX `std::rename` 原子替換，不再先刪除既有目標檔；新增 `test-posix-atomic-replacement-contract.ps1` 並接入 GitHub validation。
- Editor、Reader、PageMaster、Diff 的顯示名稱與內部設定名稱均已統一為 FamilyPDF。
- 已加入 `.github/dependabot.yml` 與 `.github/workflows/codeql.yml`。
- AES 解密已拒絕截斷、非區塊對齊與錯誤 padding；新增 `UnitTestsSecurity`，並加入 AES 靜態安全契約。
- Appx、WiX、Debian、Flatpak、AppStream 與 desktop metadata 的可見品牌／版本／來源已統一；技術 package ID 刻意保留以維持升級相容性。
- 新增跨平台 branding contract，並將其與 AES contract 接入 GitHub validation。
- Windows installer 編譯支援指定 runtime package 路徑，已避開同步工具鎖定舊 package 的本機封裝問題。
- 已建立 Inno Setup 覆蓋升級流程；保留正式 FamilyPDF AppId，0.2.2 可直接升級至 0.2.3，不需先解除安裝。
- 本機正式安裝已完成覆蓋升級，登錄版本為 `0.2.3`；安裝目錄中的主要 EXE 已與最終 package 雜湊一致。
- 最終安裝包：`dist\\FamilyPDF-Full-Setup-x64.exe`。
- 升級說明：`docs\\UPGRADE-v0.2.3.md`。

## 7. 最近驗證結果

- CTest：`7/7 passed`，包含 `UnitTestsSecurity` 的 AES-256 邊界、fresh IV、截斷 ciphertext 與錯誤 padding 測試。
- Office Export Python unittest：`11/11 passed`。
- OCR manifest、下載雜湊與竄改拒絕測試：通過。
- Branding、installer upgrade、external process timeout、toolchain path contracts：通過。
- Atomic output and TTS proxy contracts：通過；SignaturePlugin、Pdf4QtLibGui 與 Pdf4QtViewer 相關 target 建置成功。
- Qt test platform contract、`UnitTestsBookmarks` 直接執行與完整 CTest：通過（6/6）。
- TTS lifecycle null-safety contract：通過；`Pdf4QtLibGui.dll` 增量建置成功。
- 測試 CMake 重構後重新配置、建置 6 個測試 target，並通過完整 CTest 6/6。
- Office Export Python unittest：13/13 通過；原子輸出回歸測試 2/2 通過。
- POSIX atomic replacement contract：通過；Windows 分支仍使用既有 `ReplaceFileW`／`MoveFileExW`。
- PowerShell scripts 語法解析與 GitHub workflow YAML：通過。
- 隔離安裝升級測試：`0.2.2 -> 0.2.3 passed`。
- `git diff --check`：通過。
- AES security contract、cross-platform branding contract：通過。
- 1160 頁 PDF Windows locale smoke：繁中／簡中各通過 10 秒載入、回應性與記憶體採樣；正式安裝後再次通過。
- 本機現有裸安裝覆蓋升級：installer exit code `0`，登錄版本 `0.2.3`，runtime `Pdf4QtLibCore.dll` 雜湊與新 package 一致。

## 8. GitHub 狀態

- Repository：`https://github.com/nanachi1212/myfamilypdf`
- Branch：`codex/auto-ocr-v0.2.0`
- 最近提交：待本次修改提交後更新。
- 本文件的後續更新應另建提交並推送；不要把使用者秘密或未核准的個人檔案加入 Git。

最後更新：2026-08-28。
