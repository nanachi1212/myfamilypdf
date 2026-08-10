@echo off
setlocal
if "%~1"=="" (
  echo FamilyPDF OCR
  echo.
  echo Usage:
  echo   Drag a PDF onto this file
  echo   FamilyPDF-OCR.cmd input.pdf
  echo   FamilyPDF-OCR.cmd input.pdf output.ocr.pdf -Mode Simplified -Pages 1-10
  echo   Default mode automatically selects Traditional, Simplified, or mixed OCR per page.
  echo.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0FamilyPDF-OCR.ps1" %*
set "FAMILYPDF_OCR_EXIT=%ERRORLEVEL%"
if not "%FAMILYPDF_OCR_EXIT%"=="0" pause
exit /b %FAMILYPDF_OCR_EXIT%
