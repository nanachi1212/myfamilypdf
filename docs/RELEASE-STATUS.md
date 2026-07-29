# FamilyPDF 目前交付狀態

更新日期：2026-07-29

## 可直接使用的產物

| 產物 | Bytes | SHA-256 |
|---|---:|---|
| `dist\FamilyPDF-Setup-x64.exe` | 33,726,990 | `2CE35B194F70CCC8392350DF8E8BA3EF24F942D411C7C2864C689825CDB0FEBC` |
| `dist\FamilyPDF-windows-x64.zip` | 45,478,777 | `61EF736D362A7D24315379C40F298E12CD95458DE08963579895B9D7EAF423F2` |
| `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe` | 11,893,336 | `561E2AA901349157DD797F2C424FB3D7C26E0C603DB80B3511CD99C3BDE6F2A8` |
| `dist\FamilyPDF-OCR-Plugin-windows-x64.zip` | 12,252,531 | `396119A56C9C41DDA130C1269A5D461441E8D7EACAC5E2928C2CA16E4A53F17B` |

`dist\` 是本機建置產物，不提交到 Git。

## 已驗證

- CTest 4/4 通過；書籤、安全儲存、工作階段及 PDF 互通測試共 16 項通過。
- pypdf 獨立讀回標準 PDF outline：繁體中文標題、資料夾階層、文字顏色、粗體及頁面目的地均保留。
- 標準 Highlight、Underline、StrikeOut、Square、FreeText、Text 六種註解由 FamilyPDF 嚴格模式重開及 pypdf 交叉驗證通過。
- 繁體及簡體中文驗證安裝各為 exit code `0`；安裝後 Viewer、Editor、PageMaster 同時載入中文檔名與 1,160 頁 PDF，15 秒後全部為 Responding。
- 單數頁、雙數頁及 `10-20` 範圍輸出經獨立引擎驗證為 29、29、11 頁；合併 58 頁及大型 1,160 頁檔案頁數亦正確。
- 基礎封裝不含 Tesseract、語言模型、測試 EXE 或 Qt 除錯 DLL。
- OCR 外掛 0.3.0 驗證安裝 exit code `0`。
- 安裝後 OCR 單頁、雙頁流程通過：頁數不變、原檔 SHA-256 不變、輸出為 PDF、`fetch-text` 可取得文字。
- 橫排繁體中文、簡體中文及英文模型已內建並完成端對端測試。
- 缺少所選語言時，OCR 主流程會自動啟動官方下載、最多三次重試、大小檢查、Tesseract 載入驗證及原子替換；斷線測試確認不產生輸出 PDF 或殘留半檔。
- 直排繁／簡中文已納入自動下載清單，且直排繁簡測試 PDF 已建立並視覺檢查；本次 Codex 網路沙盒仍阻擋兩個模型下載，因此未內建於目前產物。

## Git

- 分支：`codex/phase0-baseline`
- push 目標：`origin/codex/phase0-baseline`
