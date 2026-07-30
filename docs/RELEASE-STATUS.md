# FamilyPDF 目前交付狀態

更新日期：2026-07-30

## 可直接使用的產物

| 產物 | Bytes | SHA-256 |
|---|---:|---|
| `dist\FamilyPDF-Setup-x64.exe` | 33,793,932 | `82549EFE94032EE401C4F2A7AA603313932A9458DBD80265A05A936917201627` |
| `dist\FamilyPDF-windows-x64.zip` | 45,631,774 | `A0CAE0D88498765434DA255FA26CB83AF1A0E4A110DA03EDEFD69B341F465C27` |
| `dist\FamilyPDF-OCR-Plugin-Setup-x64.exe` | 14,048,057 | `C7092441CDB7992B12A339D17047B0BF27B056CA0B7503FCEF531F2C4B2321A3` |
| `dist\FamilyPDF-OCR-Plugin-windows-x64.zip` | 14,657,824 | `57A33A39EFC7B1B946C49FDFAD8892E3311D0CBA3ED8EEF4A681B9284B4D4162` |

`dist\` 是本機建置產物，不提交到 Git。

## 已驗證

- CTest 4/4 通過；書籤、安全儲存、工作階段及 PDF 互通測試共 16 項通過。
- pypdf 獨立讀回標準 PDF outline：繁體中文標題、資料夾階層、文字顏色、粗體及頁面目的地均保留。
- 標準 Highlight、Underline、StrikeOut、Square、FreeText、Text 六種註解由 FamilyPDF 嚴格模式重開及 pypdf 交叉驗證通過。
- 繁體及簡體中文驗證安裝各為 exit code `0`；安裝後 Viewer、Editor、PageMaster 同時載入中文檔名與 1,160 頁 PDF，15 秒後全部為 Responding。
- 單數頁、雙數頁及 `10-20` 範圍輸出經獨立引擎驗證為 29、29、11 頁；合併 58 頁及大型 1,160 頁檔案頁數亦正確。
- 基礎封裝不含 Tesseract、語言模型、測試 EXE 或 Qt 除錯 DLL。
- OCR 外掛 0.3.0 最新隔離安裝 exit code `0`；安裝後 manifest、五個語言模型及橫排／直排 OCR 回歸均通過。
- 安裝後 OCR 單頁、雙頁流程通過：頁數不變、原檔 SHA-256 不變、輸出為 PDF、`fetch-text` 可取得文字。
- 橫排繁體中文、簡體中文及英文模型已內建；直接辨識分別取得「傳統中文測試」、「简体中文测试」與 `FamilyPDF OCR 2026`。封裝後端對端流程亦確認原檔雜湊不變、頁數不變、PDF 文字層含繁簡中文字元，且 UTF-8 文字檔保留完整字序。
- 缺少所選語言時，OCR 主流程會自動啟動官方下載、最多三次重試、大小檢查、Tesseract 載入驗證及原子替換；斷線測試確認不產生輸出 PDF 或殘留半檔。
- 直排繁／簡中文已由官方 `tessdata_fast` 成功下載、SHA-256 與 Tesseract 載入驗證通過，並已內建於目前產物。
- `EditorPlugin.dll`、`RedactPlugin.dll`、`SignaturePlugin.dll` 已進入 release 可攜包及正式安裝程式；專用 verification installer 安裝 exit code `0`，安裝後三個 DLL 的大小檢查通過。

## Git

- 分支：`codex/phase0-baseline`
- push 目標：`origin/codex/phase0-baseline`
