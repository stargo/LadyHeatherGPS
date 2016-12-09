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
AppVerName=Lady Heather's Disciplined Oscillator Control Program v5.00 of 9-Dec-16
DefaultDirName={pf}\Heather
DefaultGroupName=Lady Heather
UninstallDisplayIcon={app}\heather.exe
Compression=lzma
SolidCompression=yes
ChangesAssociations=yes

[Registry]

[Run]
Filename: "{app}\heather.pdf"; Description: "Open user guide (requires .PDF viewer)"; Flags:postinstall shellexec

[Files]
Source: "heather.iss";  DestDir: "{app}"
Source: "makefile.w32";  DestDir: "{app}"  
Source: "m.bat";  DestDir: "{app}"  
Source: "r.bat";  DestDir: "{app}"  
Source: "heather.exe";  DestDir: "{app}"
Source: "heather.cfg";  DestDir: "{app}"
Source: "heather.cfg";  DestDir: "{userdocs}"
Source: "w32sal.dll";   DestDir: "{app}"
Source: "winvfx16.dll"; DestDir: "{app}"
Source: "heather.cpp";  DestDir: "{app}"
Source: "heather.xbm";  DestDir: "{app}"
Source: "heather.xpm";  DestDir: "{app}"
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

Source: "heather_alarm.wav"; DestDir: "{app}" 
Source: "heather_bell.wav"; DestDir: "{app}" 
Source: "heather_noon.wav"; DestDir: "{app}" 
Source: "heather_notify.wav"; DestDir: "{app}" 
Source: "heather_sunrise.wav"; DestDir: "{app}" 
Source: "heather_chime.wav"; DestDir: "{app}" 
Source: "heather_cuckoo.wav"; DestDir: "{app}" 
Source: "heather_leapsec.wav"; DestDir: "{app}" 
Source: "heather_song00.wav"; DestDir: "{app}" 
Source: "heather_song15.wav"; DestDir: "{app}" 
Source: "heather_song30.wav"; DestDir: "{app}" 
Source: "heather_song45.wav"; DestDir: "{app}" 

Source: "server.cpp"; DestDir: "{app}"
Source: "serve.bat"; DestDir: "{app}"
Source: "server.ex1"; DestDir: "{app}"

Source: "readme.htm";   DestDir: "{app}"; Flags: isreadme
Source: "heather.pdf";   DestDir: "{app}";
Source: "heather.docx";   DestDir: "{app}";

[Tasks]
Name: "desktopicon"; Description: "Create &Desktop shortcuts"; GroupDescription: "Additional icons:";
Name: "quickicon"; Description: "Create &Quick Launch shortcuts"; GroupDescription: "Additional icons:";

[Icons]
Name: "{group}\Readme"; Filename: "{app}\readme.htm"
Name: "{group}\User guide"; Filename: "{app}\heather.pdf"
Name: "{group}\Lady Heather"; Filename: "{app}\heather.exe"
Name: "{userdesktop}\Lady Heather"; Filename: "{app}\heather.exe"; Tasks: desktopicon
Name: "{userdesktop}\KE5FX TBolt (Seattle, USA)"; Filename: "{app}\heather.exe"; Parameters: "/ip=ke5fx.dyndns.org"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Lady Heather (COM1)"; Filename: "{app}\heather.exe"; Tasks: quickicon
