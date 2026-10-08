# 出貨包瘦身 v16

更新日期：2026-10-08。`scripts/phase0/package-windows-runtime.ps1` 不再把開發工具放進可攜包與安裝程式。

## 移除

- `CodeGenerator.exe`（PDF 物件程式碼產生器，只有開發 PDF4QT 時用）
- `PdfExampleGenerator.exe`（產生測試 PDF 的開發工具）
- `JBIG2_VIEWER.exe`（JBIG2 影像除錯檢視器）
- `*.pdb`（除錯符號，若建置輸出有）

腳本在複製後會檢查這三個檔案不在封裝目錄內，否則失敗。安裝程式（Inno Setup）照封裝目錄打包，所以同時瘦身。

## 保留與原因

- `Qt6Multimedia.dll` 與 FFmpeg（`avcodec`／`avformat`／`avutil`／`swresample`／`swscale`）：`Qt6TextToSpeech.dll` 與三個 texttospeech 外掛直接 import `Qt6Multimedia.dll`（`dumpbin /dependents` 實測），拿掉就沒有朗讀功能。要再瘦身必須一併放棄朗讀，本輪不做。
- `Pdf4QtLaunchPad.exe`：很小，且安裝程式捷徑可能引用，不動。
- `Qt6Test.dll`：封裝腳本本來就不複製（`Qt6*.dll` 只取清單內的 10 個），先前 `dist/FamilyPDF` 內的是手動部署殘留，不需改腳本。

## 驗證

本機以 `-SkipOcr -SkipOfficeBuild` 重新執行封裝腳本：封裝目錄不含三個開發工具，仍含五個主程式與 `Pdf4QtLaunchPad.exe`。
