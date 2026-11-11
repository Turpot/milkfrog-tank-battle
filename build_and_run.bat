@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
  echo [错误] 没有找到 gcc。请先安装 MinGW-w64，并将 bin 目录加入 PATH。
  pause
  exit /b 1
)

echo 正在使用当前最新源码编译...
gcc -std=c11 -O2 -s -Wall -Wextra -Wpedantic -Werror -Isrc -Ilib/include src/main.c src/map.c src/algorithms.c src/game.c src/view.c -o TankBattle90.exe -Llib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows

if errorlevel 1 (
  echo.
  echo [失败] 编译未完成，请把完整报错截图发给我。
  pause
  exit /b 1
)

if not exist TankBattle90.exe (
  echo [失败] 没有生成 TankBattle90.exe。
  pause
  exit /b 1
)

echo [成功] 最新源码编译完成，正在启动游戏...
start "" "%~dp0TankBattle90.exe"
exit /b 0
