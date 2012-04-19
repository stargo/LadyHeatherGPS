@echo off
:start
if not exist server.ex1 goto serve
copy server.ex1 server.exe >nul
del server.ex1
:serve
if not exist server.exe goto bail
server %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 2 goto start
:bail
