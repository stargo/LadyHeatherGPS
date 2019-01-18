###############################################################
#							      #
#	Lady Heather MAKEFILE for Linux, FreeBSD, or	      #
#	OS/X (with XQuartz X11 library) 		      #
#							      #
#							      #
###############################################################

OS := $(shell uname -s)
CC = g++

#
# Linux build
#

ifeq ($(OS),Linux)

# WARNS = -Wno-write-strings 
WARNS = -Wno-write-strings -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-format-overflow

all: heather

heather.o: heather.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heather.cpp $(WARNS)

heathmsc.o: heathmsc.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathmsc.cpp $(WARNS)

heathui.o: heathui.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathui.cpp $(WARNS)

heathgps.o: heathgps.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathgps.cpp $(WARNS)

heather: heather.o heathmsc.o heathui.o heathgps.o
		  $(CC) heather.o heathui.o heathgps.o heathmsc.o -o heather -lm -lX11

clean:
		  rm heather.o heathui.o heathgps.o heathmsc.o heather

endif


#
# OS/X build
# 

ifeq ($(OS),Darwin)

# WARNS = -Wno-write-strings 
WARNS = -Wno-write-strings -Wall -Wno-unused-variable 

all: heather

heather.o: heather.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heather.cpp $(WARNS) -I/opt/X11/include

heathmsc.o: heathmsc.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathmsc.cpp $(WARNS) -I/opt/X11/include 

heathui.o: heathui.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathui.cpp $(WARNS) -I/opt/X11/include 

heathgps.o: heathgps.cpp heather.ch heathfnt.ch makefile
		  $(CC) -c heathgps.cpp $(WARNS) -I/opt/X11/include 

heather: heather.o heathmsc.o heathui.o heathgps.o
		  $(CC) heather.o heathui.o heathgps.o heathmsc.o -o heather -L/usr/X11/lib -lm -lX11

clean:
		  rm heather.o heathui.o heathgps.o heathmsc.o heather

endif



#
#  FreeBSD build
#

ifeq ($(OS),FreeBSD)
CC = cc
WARNS = -Wall -Wno-c++11-compat-deprecated-writable-strings
INCLUDES = -I /usr/local/include
LIBS = -L/usr/local/lib -lm -lX11

.SUFFIXES:
.SUFFIXES: .cpp .o

.cpp.o:
	$(CC) $(WARNS) $(INCLUDES) -c $< -o $@.new
	mv $@.new $@

SRCS = heather.cpp heathmsc.cpp heathui.cpp heathgps.cpp
OBJS = $(SRCS:cpp=o)

all: heather

heather: $(OBJS)
	$(CC) -o $@.new $(OBJS) $(LIBS)
	mv $@.new $@

$(OBJS): heather.ch heathfnt.ch makefile

clean:
	rm -f heather heather.new $(OBJS) $(OBJS:=.new)
endif
