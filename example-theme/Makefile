CC=gcc
CFLAGS=-g -Wall -fPIC
LDFLAGS=
INCLUDES=

COMPILE = $(CC) $(INCLUDES) $(CFLAGS)
LINK = $(CC) $(CFLAGS) $(LDFLAGS)

INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_PROGRAM = $(INSTALL) -m 755

eft-theme.so: eft_1024_576_cropped.png.c.o eft_1024_768.png.c.o eft_800_600.png.c.o throbber_fore.png.c.o throbber_back.png.c.o eft-theme.c.o helvB10.bdf.c.o
	$(COMPILE) -shared -o $@ $^
	
%.png.c: %.png
	pngtousplash $< > $@

%.bdf.c: %.bdf
	bdftousplash $< > $@

%.c.o: %.c
	$(COMPILE) -o $@ -c $<

install:
	$(INSTALL) -d $(DESTDIR)/usr/lib/usplash
	$(INSTALL_PROGRAM) eft-theme.so $(DESTDIR)/usr/lib/usplash/eft-theme.so
clean:
	rm -f *.png.c *.bdf.c *.c.o *.so
