@echo off
set "TOOLS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64"
"%TOOLS%\lib.exe" /nologo /machine:x64 /def:tests\hostcrt\ucrt.def /out:tests\output\hostcrt.lib
"%TOOLS%\lib.exe" /nologo /machine:x64 /def:tests\hostcrt\kernel.def /out:tests\output\kernel.lib
"%TOOLS%\cl.exe" /nologo /std:c11 /W3 /wd4244 /O2 /GS- /Zl /Itests\hostcrt /c /Fo:tests\output\simulation.obj tests\simulation_runner.c
if errorlevel 1 exit /b 1
"%TOOLS%\cl.exe" /nologo /std:c11 /W3 /wd4244 /O2 /GS- /Zl /Itests\hostcrt /c /Fo:tests\output\hostcrt.obj tests\hostcrt\hostcrt.c
if errorlevel 1 exit /b 1
"%TOOLS%\link.exe" /nologo /entry:main /subsystem:console /nodefaultlib /out:tests\output\simulation.exe tests\output\simulation.obj tests\output\hostcrt.obj tests\output\hostcrt.lib tests\output\kernel.lib
if errorlevel 1 exit /b 1
tests\output\simulation.exe

