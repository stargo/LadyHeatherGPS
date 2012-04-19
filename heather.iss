; -- GPIB.iss --
; Inno Setup script file for Heather Windows port
;
; See www.jrsoftware.com/isinfo.php for more information on
; Inno Setup
;

[Languages]
Name: mytrans; MessagesFile: "default.isl"

[Setup]
AppName=Lady Heather's Disciplined Oscillator Control Program
AppVerName=Lady Heather's Disciplined Oscillator Control Program v3.10 of 18-Jun-12
DefaultDirName={pf}\Heather
DefaultGroupName=Lady Heather
UninstallDisplayIcon={app}\heather.exe
Compression=lzma
SolidCompression=yes
ChangesAssociations=yes

[Registry]

[Files]
Source: "heather.iss";  DestDir: "{app}"
Source: "makefile";  DestDir: "{app}"  
Source: "m.bat";  DestDir: "{app}"  
Source: "r.bat";  DestDir: "{app}"  
Source: "heather.exe";  DestDir: "{app}"
Source: "w32sal.dll";   DestDir: "{app}"
Source: "winvfx16.dll"; DestDir: "{app}"
Source: "heather.cpp";  DestDir: "{app}"
Source: "heathgps.cpp";  DestDir: "{app}"
Source: "heathmsc.cpp";  DestDir: "{app}"
Source: "heathui.cpp";  DestDir: "{app}"
Source: "heather.ch";  DestDir: "{app}"
Source: "heathfnt.ch";  DestDir: "{app}"
Source: "heather.cal";  DestDir: "{app}"
Source: "ipconn.cpp";   DestDir: "{app}" 
Source: "timeutil.cpp";   DestDir: "{app}" 
Source: "resource.h";   DestDir: "{app}" 
Source: "typedefs.h";   DestDir: "{app}" 
Source: "sal.h";   DestDir: "{app}" 
Source: "winvfx.h";   DestDir: "{app}" 
Source: "stdafx.h";   DestDir: "{app}" 
Source: "heather.ico";  DestDir: "{app}"  
Source: "heather.rc";   DestDir: "{app}" 
Source: "heather.res";   DestDir: "{app}" 
Source: "winvfx16.lib";   DestDir: "{app}" 
Source: "w32sal.lib";   DestDir: "{app}" 
Source: "default.isl"; DestDir: "{app}"
Source: "scrn.png"; DestDir: "{app}"
Source: "props.gif"; DestDir: "{app}"
Source: "propst.gif"; DestDir: "{app}"
Source: "HEATHDOS.EXE"; DestDir: "{app}"

Source: "server.cpp"; DestDir: "{app}"
Source: "serve.bat"; DestDir: "{app}"
Source: "server.ex1"; DestDir: "{app}"

Source: "readme.htm";   DestDir: "{app}"; Flags: isreadme

[Tasks]
Name: "desktopicon"; Description: "Create &Desktop shortcuts"; GroupDescription: "Additional icons:";
Name: "quickicon"; Description: "Create &Quick Launch shortcuts"; GroupDescription: "Additional icons:";

[Icons]
Name: "{group}\Readme"; Filename: "{app}\readme.htm"
Name: "{group}\Lady Heather"; Filename: "{app}\heather.exe"
Name: "{userdesktop}\Lady Heather"; Filename: "{app}\heather.exe"; Tasks: desktopicon
Name: "{userdesktop}\KE5FX TBolt (Seattle, USA)"; Filename: "{app}\heather.exe"; Parameters: "/ip=ke5fx.dyndns.org /gb"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Lady Heather (COM1)"; Filename: "{app}\heather.exe"; Tasks: quickicon

