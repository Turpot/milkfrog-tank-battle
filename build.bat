@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo   奶蛙坦克大战 - 最新源码编译
echo ==========================================
echo.

where gcc >nul 2>nul
if errorlevel 1 (
  echo [错误] 没有找到 gcc。
  echo 请先安装 MinGW-w64 GCC，并将其 bin 目录加入 PATH。
  echo 安装后重新打开命令提示符，再运行本脚本。
  pause
  exit /b 1
)

echo [1/2] 正在编译最新源码...
gcc -std=c11 -O2 -s -Wall -Wextra -Wpedantic -Werror -Isrc -Ilib/include src/main.c src/map.c src/algorithms.c src/game.c src/view.c -o TankBattle90.exe -Llib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows

if errorlevel 1 (
  echo.
  echo [失败] 编译未完成，请检查上方 GCC 报错信息。
  pause
  exit /b 1
)

if not exist TankBattle90.exe (
  echo [失败] 编译命令结束，但没有找到 TankBattle90.exe。
  pause
  exit /b 1
)

echo.
echo [成功] 已由当前目录的最新源码生成 TankBattle90.exe。
echo 请保留 assets 文件夹，并从当前目录启动游戏。
pause
exit /b 0
