###############################################################
#                                                             #
#  MAKEFILE for Win32                                         #
#                                                             #
#  Execute with Microsoft (or compatible) NMAKE               #
#                                                             #
###############################################################

CPU = i386
TARGETOS = WIN95

optflag = $(cdebug)
appflags = /D_WIN32_IE=0x400 /D_CRT_SECURE_NO_DEPRECATE /D_USE_32BIT_TIME_T

!include <ntwin32.mak>

all: heather.exe server.pdb

#
# Client
#

heather.res: heather.ico heather.rc resource.h
   rc heather.rc

heather.exe: heather.obj heathgps.obj heathmsc.obj heathui.obj winvfx16.lib w32sal.lib heather.res
    $(link) $(ldebug) $(guilflags) -out:heather.exe -stack:20971520 heathui.obj heather.obj heathgps.obj heathmsc.obj w32sal.lib winvfx16.lib $(guilibsmt) winmm.lib shell32.lib heather.res

heathgps.obj: heathgps.cpp sal.h winvfx.h heather.ch makefile
    $(cc) $(optflag) $(cflags) $(cvarsmt) $(appflags) heathgps.cpp

heathmsc.obj: heathmsc.cpp sal.h winvfx.h heather.ch makefile
    $(cc) $(optflag) $(cflags) $(cvarsmt) $(appflags) heathmsc.cpp

heathui.obj: heathui.cpp sal.h winvfx.h heather.ch makefile
    $(cc) $(optflag) $(cflags) $(cvarsmt) $(appflags) heathui.cpp

heather.obj: heather.cpp ipconn.cpp timeutil.cpp sal.h winvfx.h heather.ch makefile
    $(cc) $(optflag) $(cflags) $(cvarsmt) $(appflags) heather.cpp

#
# Server
# (saved as server.ex1, so that serve.bat will rename it to server.exe)
#

server.pdb: server.obj
    $(link) $(ldebug) $(conflagsmt) -out:server.ex1 -incremental:no server.obj $(conlibsmt) winmm.lib user32.lib

server.obj: server.cpp ipconn.cpp makefile
    $(cc) $(cdebug) $(cflags) $(cvarsmt) $(appflags) server.cpp

#
# Client setup program (requires Inno Setup installation, see
# www.jrsoftware.org)
#

output\setup.exe: heather.iss
   c:\progra~2\innose~1\iscc /Q heather.iss

