@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ocr\Install-OCR-Languages.ps1"
set "FAMILYPDF_OCR_EXIT=%ERRORLEVEL%"
if not "%FAMILYPDF_OCR_EXIT%"=="0" pause
exit /b %FAMILYPDF_OCR_EXIT%
