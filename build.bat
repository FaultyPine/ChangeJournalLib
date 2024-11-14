@echo off
setlocal

for %%a in (%*) do set "%%a=1"
set cwd=%~dp0



set LINKER_ARGS=-Xlinker /SUBSYSTEM:CONSOLE -luser32
set sources=main.cpp
set includes=
clang %sources% -o main.exe -g -D_CONSOLE -mconsole %LINKER_ARGS% %includes%



if "%run%"=="1" (
    main.exe
)

endlocal