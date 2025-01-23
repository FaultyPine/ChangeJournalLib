@echo off
setlocal

for %%a in (%*) do set "%%a=1"
set cwd=%~dp0



set LINKER_ARGS=-Xlinker /SUBSYSTEM:CONSOLE -luser32
set sources=test.cpp change_journal.cpp
set includes=
clang %sources% -O1 -o main.exe -g -D_CONSOLE -mconsole %LINKER_ARGS% %includes% -w -std=c++17
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit /b)



if "%run%"=="1" (
    main.exe
)

endlocal