@echo off
setlocal
rem By Infinest (Townfall Super Ultrawide). Modified 2026-10-17 by
rem it-was-katsumata: winmm default target, VS_ROOT, config.cpp. GPL-3.0.
rem VS_ROOT defaults to the VS 2022 Build Tools install.
if "%VS_ROOT%"=="" set VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

rem Build the mod as a proxy for a system DLL the game imports statically.
rem   build.bat            -> winmm.dll (default; coexists with OptiScaler's dxgi.dll)
rem   build.bat dsound     -> dsound.dll
rem   build.bat dxgi       -> dxgi.dll (only if nothing else uses that name)
rem Any system DLL whose export table is fully named (no ordinal-only or
rem forwarded exports) works; the stub list is generated from the real DLL.
set TARGET=%~1
if "%TARGET%"=="" set TARGET=winmm
if /i "%TARGET:~-4%"==".dll" set TARGET=%TARGET:~0,-4%

set ROOT=%~dp0
set OUT=%ROOT%\dist
set GEN=%OUT%\gen
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%GEN%" mkdir "%GEN%"

set REALDLL=%SystemRoot%\System32\%TARGET%.dll
if not exist "%REALDLL%" (echo Unknown target: %REALDLL% not found & exit /b 1)

python -I "%ROOT%\tools\gen_proxy.py" "%REALDLL%" "%GEN%\%TARGET%_stubs.cpp" "%GEN%\%TARGET%.def" || exit /b 1

cl /nologo /MT /O2 /GL /W4 /GS- ^
   /I"%ROOT%\src" ^
   /DPROXY_TARGET_DLL=\"%TARGET%.dll\" ^
   /Fe:"%OUT%\%TARGET%.dll" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   "%ROOT%\src\dllmain.cpp" "%ROOT%\src\config.cpp" ^
   "%GEN%\%TARGET%_stubs.cpp" ^
   "%ROOT%\src\proxy.cpp" ^
   "%ROOT%\src\log.cpp" "%ROOT%\src\detour.cpp" "%ROOT%\src\hooks.cpp" ^
   /link /nologo /DLL /OPT:REF /OPT:ICF /LTCG /DEF:"%GEN%\%TARGET%.def" user32.lib kernel32.lib || exit /b 1

echo.
echo Built: %OUT%\%TARGET%.dll
echo Install: copy %OUT%\%TARGET%.dll into
echo   ^<Steam library^>\steamapps\common\Townfall\Townfall\Binaries\Win64\
