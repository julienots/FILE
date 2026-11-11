@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist .venv\Scripts\python.exe (
  echo Lancez d'abord install_windows.bat
  pause
  exit /b 1
)
echo Mode reseau local : le telephone peut se connecter (autorisez l'acces si le pare-feu Windows le demande).
.venv\Scripts\python.exe -m superessence --lan %*
pause
