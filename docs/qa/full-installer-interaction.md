# 一鍵完整安裝程式雙語 GUI 驗證

更新日期：2026-07-30

## 測試對象

使用與正式版相同 payload 的
`build\FamilyPDF-Full-Verification-Setup-x64.exe`，分別以：

```text
/LANG=chinesetraditional
/LANG=chinesesimplified
```

啟動互動式安裝畫面。驗證版只停用開始功能表、登錄與解除安裝項目，元件
選擇及檔案 payload 與正式版相同。

## 繁體中文

- 安裝標題、說明、按鈕及結束確認均顯示繁體中文。
- 安裝類型正確顯示「完整安裝（建議）」、「僅安裝 PDF 閱讀與編輯」及
  「自訂安裝」。
- 完整安裝預設勾選「OCR 可搜尋文字（繁體、簡體、英文及直排模型）」。
- 切換成精簡安裝後 OCR 取消勾選，需求空間由 196.0 MB 降為 168.7 MB。
- 切回完整安裝後 OCR 自動恢復勾選。

## 簡體中文

- 安裝標題、說明、按鈕及退出確認均顯示簡體中文。
- 安裝類型正確顯示「完整安装（建议）」、「仅安装 PDF 阅读与编辑」及
  「自定义安装」。
- 元件說明顯示「FamilyPDF 阅读、编辑、表单与 Office 导出」及
  「OCR 可搜索文字（繁体、简体、英文及竖排模型）」。
- 完整模式預設勾選 OCR；精簡模式取消 OCR；切回完整模式後恢復勾選。

## Windows 整合選項

- 繁中畫面顯示「Windows 整合」及「加入 PDF 右鍵『使用 FamilyPDF
  開啟／編輯』及 Windows『開啟方式』」。
- 簡中畫面顯示「Windows 集成」及「添加 PDF 右键『使用 FamilyPDF
  打开／编辑』及 Windows『打开方式』」。
- 兩種語系均預設勾選 Windows 整合；桌面捷徑仍預設不勾選。
- 驗證安裝器不含正式 Registry 寫入段落，因此 GUI 檢查不會修改目前電腦
  的檔案關聯。

## 其他自動證據

`scripts\qa\smoke-full-installer.ps1` 另以隔離目錄實際完成兩種元件組合：

- `core,ocr`：主程式、五個 OCR 模型、Viewer Responding，以及繁簡中
  可搜尋 PDF 回歸均通過。
- `core`：主程式可用，且確認沒有 OCR 執行檔、腳本或語言模型。

結果寫入 `build\full-installer-smoke\summary.json`。

GUI 檢查完成後兩個安裝器均由「取消／退出」正常關閉，沒有寫入安裝目錄。
