# OCR Spike

這是獨立於 PDF4QT 主程式的 OCR 依賴驗證區，使用 vcpkg 的 Tesseract 5.5.2 與 Leptonica。完成安裝後會驗證：

1. `tesseract.exe --version`
2. 英文與繁體／簡體中文語言資料是否可載入。
3. 將掃描頁轉成文字結果，評估後續 PDF 隱形文字層所需的座標資料。

目前不直接修改 PDF4QT 的保存格式；OCR 的文字層、座標轉換、旋轉頁與安全保存需在 spike 結果後再決定。
