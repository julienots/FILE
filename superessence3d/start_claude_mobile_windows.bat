@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist .venv\Scripts\python.exe (
  echo Lancez d'abord install_windows.bat
  pause
  exit /b 1
)
where cloudflared >nul 2>nul
if errorlevel 1 (
  echo Installation de cloudflared (tunnel HTTPS gratuit de Cloudflare)...
  winget install --id Cloudflare.cloudflared -e --accept-source-agreements --accept-package-agreements
  echo Si l'installation vient de se terminer, fermez cette fenetre et relancez ce fichier.
)
.venv\Scripts\python.exe -m superessence.mcp_remote %*
pause
