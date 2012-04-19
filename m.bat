@echo off
if exist c:\dev\sdcc\sdcc.ico goto inhouse_build
nmake /f makefile
goto bail

:inhouse_build
call setset c14
nmake /f makefile
:bail
