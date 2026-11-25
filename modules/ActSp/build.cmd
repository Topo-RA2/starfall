@echo off
setlocal
cd /d "%~dp0..\.."
call tools\msvc-x86.cmd || exit /b 1
if not exist build\obj\ActSp mkdir build\obj\ActSp || exit /b 1
cl /nologo /std:c++20 /EHsc /O2 /MT /LD /utf-8 /DSYR_VER=2 /DNOMINMAX /Ireferences\Phobos\YRpp /Fobuild\obj\ActSp\ /Febuild\ActSp.dll modules\ActSp\Drones.cpp modules\ActSp\GuardPoints.cpp modules\ActSp\GroundGuards.cpp modules\ActSp\BetterGarrison.cpp /link /IMPLIB:build\obj\ActSp\ActSp.lib /INCREMENTAL:NO
exit /b %errorlevel%
