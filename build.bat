@echo off
echo Compiling resources...
windres resource.rc -O coff -o resource.res
if errorlevel 1 goto error

echo Compiling AutoClicker with GCC...
gcc -O2 -s -mwindows -o autoclicker.exe main.c resource.res -luser32 -lgdi32 -lcomctl32 -lwinmm
if errorlevel 1 goto error

echo Build successful! Binary size:
dir autoclicker.exe | findstr autoclicker.exe
goto end

:error
echo Build failed!
exit /b 1

:end
