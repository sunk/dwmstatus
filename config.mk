NAME = dwmstatus
VERSION = 1.0

# Customize below to fit your system

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

CPPFLAGS = -DVERSION=\"${VERSION}\" -D_DEFAULT_SOURCE
CFLAGS = -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}
#LDFLAGS = -s ${LIBS}

# compiler and linker
CC = cc
