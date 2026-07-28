# Phase 0 PDF4QT 基線建置結果

更新日期：2026-07-28

## 結果

- Configure：成功
- Build：成功，185/185 個步驟完成
- Test：成功，3/3 通過，0 failures
- 執行檔位置：`build/phase0-upstream-release/usr/bin/`

已驗證的測試：

- `UnitTests`
- `UnitTestsImageOptimizer`
- `UnitTestsFontEncoding`

## 執行期依賴部署

第一次測試時，測試執行目錄缺少 Qt 與第三方 DLL，Windows 回報 `0xc0000135`。已在 `scripts/phase0/build-upstream-baseline.ps1` 加入自動處理：

1. 對三個測試執行 `windeployqt`，部署 Qt DLL 與平台／影像插件。
2. 從 build 目錄的 vcpkg manifest 安裝結果複製第三方 DLL，例如 `lcms2-2.dll`、`tbb12.dll`、OpenSSL DLL。
3. 再次執行測試後三組全部通過。

## 邊界

這是未修改 PDF4QT 的基線驗證，不代表家庭版客製功能已完成。OCR、繁／簡中文介面確認、大檔案效能測試、安裝包與安全儲存流程仍在後續階段。
