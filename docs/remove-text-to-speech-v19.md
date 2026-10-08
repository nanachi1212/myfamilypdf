# 移除朗讀功能 v19

更新日期：2026-10-08。依使用者決定（不需要朗讀）移除文字轉語音。

## 移除內容

- `Pdf4QtLibGui/pdftexttospeech.*`（Qt TextToSpeech 包裝）；側邊欄 Speech 頁不再顯示；設定對話框的 Speech 頁籤從清單移除。
- 不再連結 `Qt6::TextToSpeech`，也不再間接依賴 `Qt6Multimedia` 與 FFmpeg。可攜包與安裝程式不含 `Qt6TextToSpeech.dll`、`Qt6Multimedia.dll`、`texttospeech\`、`multimedia\` 外掛，Qt SBOM 清單少 `qtmultimedia`、`qtspeech`，授權說明移除 FFmpeg 段落。
- 設定檔的 `speech*` 欄位保留不讀寫介面（舊設定檔照常載入，不會出錯）。

## 保留

- `AudioBookPlugin` 與 `PdfTool audiobook`：不依賴 Qt TextToSpeech（用平台語音 API），本輪不動。
- `.ui` 內未使用的 Speech 控制項沒有刪（避免大量 Designer XML 變動）。
- `WixInstaller/Product.wxs.in`（上游 MSI 範本，FamilyPDF 不使用）仍列出這些 DLL；若日後要用 MSI 需一併清理。

## 驗證

本機 Release 重建 BUILD_OK；`UnitTestsViewer searchExperience（viewer／editor）、splitDocumentWorkflow、thumbnailSelectionAndPageManagement` PASS；`Pdf4QtLibGui.dll` 的 Qt 相依不含 TextToSpeech／Multimedia。
