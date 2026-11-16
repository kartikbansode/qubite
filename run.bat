@echo off
echo ========================================================
echo   Starting Cafe Token Management System
echo ========================================================
echo.
echo Local Web Access:   http://localhost:8080
echo For Kitchen Laptop: Open http://^<YOUR-IP^>:8080 on the same Wi-Fi
echo.
build\CafeServer.exe --port 8080 --webroot frontend --db data/cafeteria.db
pause
