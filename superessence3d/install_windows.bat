@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
echo.
echo  === SUPERESSENCE 3D STUDIO : installation ===
echo.
where uv >nul 2>nul
if errorlevel 1 (
  echo [1/4] Installation de l'outil "uv" (gestionnaire Python gratuit, Astral)...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "irm https://astral.sh/uv/install.ps1 | iex"
  set "PATH=%USERPROFILE%\.local\bin;%PATH%"
)
echo [2/4] Creation de l'environnement Python 3.13...
uv venv --python 3.13 .venv || goto :erreur
echo [3/4] Installation de Blender (module bpy, ~400 Mo) et des composants...
uv pip install --python .venv\Scripts\python.exe -e . pytest || goto :erreur
echo [4/4] Configuration de Claude Desktop...
.venv\Scripts\python.exe scripts\make_claude_config.py --install
echo.
echo  Installation terminee. Lancez SUPERESSENCE avec start_windows.bat
pause
exit /b 0
:erreur
echo.
echo  L'installation a echoue. Verifiez votre connexion Internet puis relancez ce fichier.
pause
exit /b 1
