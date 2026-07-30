# Office Export 驗收 fixture

測試以結構化 extraction model 建立 fixture，避免把 Windows 商用中文字型檔提交到 Git。

- DOCX fixture：兩頁，含繁體中文、簡體中文、英數、粗體、斜體、兩段文字與分頁。
- XLSX fixture：兩頁表格，含繁簡中文、合併儲存格，以及沒有表格時的逐行 fallback。
- PDF extraction 整合 fixture 會由測試程式在暫存資料夾建立；掃描 PDF／無文字層另測錯誤與 OCR 提示。
