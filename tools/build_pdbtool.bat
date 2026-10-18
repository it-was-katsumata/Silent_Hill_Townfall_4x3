@echo off
rem Builds tools\pdbtool.exe. VS_ROOT defaults to the VS 2022 Build Tools install.
if "%VS_ROOT%"=="" set VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set DIASDK=%VS_ROOT%\DIA SDK
cl /EHsc /O2 /nologo /MT "%~dp0pdbtool.cpp" /Fo:"%~dp0\" /I"%DIASDK%\include" /link /nologo "%DIASDK%\lib\amd64\diaguids.lib" ole32.lib oleaut32.lib /OUT:"%~dp0pdbtool.exe" || exit /b 1
