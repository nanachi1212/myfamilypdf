# Office Export 第三方套件

FamilyPDF Office Export 使用下列直接依賴。確切版本及所有間接依賴記錄於
`requirements.lock`，封裝時保留各套件的 `.dist-info` 授權檔。

| 套件 | 版本 | 授權 | 用途 |
|---|---:|---|---|
| pypdf | 6.14.2 | BSD-3-Clause | 測試 fixture 與 PDF 互通驗證 |
| pdfplumber | 0.11.10 | MIT | 文字層與表格擷取 |
| python-docx | 1.2.0 | MIT | DOCX 寫入與重讀 |
| openpyxl | 3.1.5 | MIT | XLSX 寫入與重讀 |
| PyInstaller | 6.21.0 | GPL-2.0-or-later with bootloader exception | 建立不需另裝 Python 的 Windows helper |

這些依賴不包含 AGPL 元件。PyInstaller 的 bootloader exception 允許散布
由 PyInstaller 建立的應用程式；本專案仍保留套件本身的授權與 notices。
