@echo off
setlocal DisableDelayedExpansion
if "%~1"=="" (
  echo FamilyPDF OCR
  echo Usage: FamilyPDF-OCR.cmd input.pdf [output.ocr.pdf] [-Mode Auto] [-Pages 1-10]
  exit /b 1
)
set "FAMILYPDF_OCR_SCRIPT=%~dp0FamilyPDF-OCR.ps1"
set "FAMILYPDF_OCR_ARG_COUNT=0"
:collect
if "%~1"=="" goto execute
set "FAMILYPDF_OCR_ARG_%FAMILYPDF_OCR_ARG_COUNT%=%~1"
set /a FAMILYPDF_OCR_ARG_COUNT+=1 >nul
shift
goto collect
:execute
setlocal EnableDelayedExpansion
set /a FAMILYPDF_OCR_ARG_LAST=FAMILYPDF_OCR_ARG_COUNT-1 >nul
set "FAMILYPDF_OCR_FORWARD="
for /l %%i in (0,1,!FAMILYPDF_OCR_ARG_LAST!) do set "FAMILYPDF_OCR_FORWARD=!FAMILYPDF_OCR_FORWARD! "!FAMILYPDF_OCR_ARG_%%i!""
rem Delayed expansion happens after cmd parses operators; filenames are never reparsed as commands.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "!FAMILYPDF_OCR_SCRIPT!" !FAMILYPDF_OCR_FORWARD!
exit /b !ERRORLEVEL!
