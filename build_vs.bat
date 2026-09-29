@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || echo "No VS vcvars; skip build"
if errorlevel 1 exit /b 1
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -10
cmake --build build --config Debug --target Isometric3DRPG 2>&1 | tail -30
