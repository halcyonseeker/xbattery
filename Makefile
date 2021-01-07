CC      = gcc
ERFLAGS = -Wall -Werror -Wpedantic
CFLAGS  = -I/usr/include/freetype2/
LDFLAGS = -lX11 -lXft -L/usr/lib/X11R6/

all: xbattery

xbattery: clean
	$(CC) xbattery.c -o xbattery $(CFLAGS) $(LDFLAGS) $(ERFLAGS)

clean:
	rm -f ./xbattery

install:
	cp ./xbattery /usr/local/bin/
