@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

REM Redirect /?, /h, etc. to help.
for %%a in (./ .- .) do if ".%~1." == "%%a?." goto Usage
for %%a in (./ .- .) do if ".%~1." == "%%ah." goto Usage

if "%1" == "elevated" (
    REM Remove Zone.Identifier in case the script was marked as downloaded from the Internet
    powershell -Command "Get-ChildItem %~dp0 -Include *.ps1,*.psm1 -Recurse | Unblock-File" > nul

    powershell -ExecutionPolicy Bypass -File "%~dp0Trace.ps1" 

) else (
    powershell -Command "Start-Process %0 -Verb Runas -ArgumentList 'elevated %1 %2 %3 %4 %5 %6 %7 %8'"
)

goto :EOF

:Usage
echo.
echo Capture ETW traces.
echo.
echo A variety of ETW provider types are supported:
echo TraceLogging, EventSource, WPP, one-line ETW.
echo.
echo USAGE:
echo     Trace             Capture trace for multimedia scenario
echo.
