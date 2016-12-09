@echo off
call setset c14
nmake /f makefile.w32 /A
if errorlevel 1 goto bail 
touch heather.iss
if errorlevel 1 goto bail 
nmake /f makefile.w32 output/setup.exe
if errorlevel 1 goto bail 
cd output
pause
call setset c16
call sign setup.exe
if errorlevel 1 goto bail 
call ke5fx.bat setup.exe heather
cd ..
call ke5fx.bat heatherx11.zip heather
call ke5fx.bat readme.txt heather
call ke5fx.bat readme.htm heather
call ke5fx.bat heather.txt heather
call ke5fx.bat heather.pdf heather
:bail

