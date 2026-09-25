# FamilyPDF 接手說明書

> 給接手本資料夾的其他 AI：先讀本文件，再讀列出的規則與入口。本文是目前盤點摘要；開始工作前仍須重新確認 Git 狀態、檔案時間與可執行環境。

## 1. 這裡是什麼

家庭用 Windows x64 PDF 閱讀／編輯工具。

## 2. 目前已知入口與邊界

AGENTS.md、README.md、docs/；不要未經要求執行 build。先看 docs/WORKSPACE-HANDOFF.md 和目前 Git 狀態。

## 3. 接手順序

1. 確認目前工作目錄是含有本檔的 repository 根目錄。
2. 若存在 AGENTS.md、CLAUDE.md、README.md 或 README.MD，按順序讀完相關規則，再讀專案入口與測試。
3. 執行唯讀盤點：git status --short --branch、git remote -v、git log -1 --oneline；若不是 Git repository，明確記錄這一點。
4. 先提出「目標、影響檔案、驗證命令、風險」。一般 commit、push、PR、CI、review 與 squash merge 不需另外確認；破壞性刪除、安裝軟體、修改 secrets／credentials、production deployment／正式 release 須先確認。
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

## 6. 建置與測試注意事項

- 建置工具由 repository 同層的 `FamilyPDF-tools` 提供，也可用 `FAMILYPDF_TOOLS_ROOT` 覆寫。
- 主要開發分支為 `main`。
- 本機直接執行 Qt 測試可能被 Windows Code Integrity 阻擋 workspace 內未簽署的第三方 DLL；不要停用安全功能，完整 runtime 驗證以 GitHub runner 為準。
- 版本以 `VERSION` 與 `CHANGELOG.md` 為準，測試結果以最新的 GitHub Actions 為準，歷史以 Git 為準，本文不記錄。
