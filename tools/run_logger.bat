@echo off
cd /d "%~dp0"
echo ESP32-CAM TTL 日志记录器
echo =======================
echo.
python ttl_logger.py %1 %2
pause
