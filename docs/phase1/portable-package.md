# Windows 可攜式套件

基礎套件：

```text
dist\FamilyPDF-windows-x64.zip
```

完整解壓後可直接執行 `Pdf4QtViewer.exe`、`Pdf4QtEditor.exe`、`Pdf4QtPageMaster.exe` 與 `PdfTool.exe`。`portable.mode` 會讓書籤、工作階段與復原資料寫入程式旁的 `data`，不污染使用者設定。

OCR 外掛：

```text
dist\FamilyPDF-OCR-Plugin-windows-x64.zip
```

把 OCR ZIP 的內容覆蓋解壓到基礎套件資料夾。完成後 Viewer／Editor 的工具選單即可啟動 OCR，也可把 PDF 拖曳到 `FamilyPDF-OCR.cmd`。

基礎 ZIP 不含 Tesseract、語言模型、單元測試或 Qt 除錯 DLL。請勿只複製單一 EXE；Qt runtime 與 plugins 必須一起保留。
