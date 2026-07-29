# FamilyPDF 目前交付狀態

更新日期：2026-07-29

## 可直接使用的產物

| 產物 | Bytes | SHA-256 |
|---|---:|---|
| `dist\FamilyPDF-Setup-x64.exe` | 33,671,717 | `782162D6718C354AAD9404DA3DC8A2D91D857EC8B1F8B5ADEE0B43BA9650B73F` |
| `dist\FamilyPDF-windows-x64.zip` | 45,378,615 | `40E8A3412C3A13824E7ADA4DC4DABF0E0FC51751D113105A6776FD078D95595D` |
| `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe` | 11,893,984 | `21F2ABB3B0F9B73C439E79D30353421FED2AB054FE938299C2C06F2EB4866B91` |
| `dist\FamilyPDF-OCR-Plugin-windows-x64.zip` | 12,251,407 | `9E148CAA11E082C1E005889D67C8726382C244FA1AF58C709867E05BA982E702` |

`dist\` 是本機建置產物，不提交到 Git。

## 已驗證

- CTest 4/4 通過。
- Viewer／Editor 由最終基礎封裝載入 1,160 頁 PDF，15 秒後皆為 Responding。
- 基礎封裝不含 Tesseract、語言模型、測試 EXE 或 Qt 除錯 DLL。
- OCR 外掛驗證安裝 exit code `0`。
- 安裝後 OCR 單頁、雙頁流程通過：頁數不變、原檔 SHA-256 不變、輸出為 PDF、`fetch-text` 可取得文字。
- 橫排繁體中文、簡體中文及英文模型已內建並完成端對端測試。
- 直排繁／簡中文已納入自動下載清單；本次 Codex 網路沙盒阻擋兩個檔案的下載，因此未內建於目前產物。

## Git

- 非 OCR 完成版 commit：`6429a117`
- OCR 外掛完成版：目前分支最新 commit。
- 若執行環境允許連線，push 目標為 `origin/codex/phase0-baseline`。
