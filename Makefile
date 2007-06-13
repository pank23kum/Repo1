CC=gcc
CFLAGS=-g -Wall -fPIC
LDFLAGS=
CPPFLAGS=-D_GNU_SOURCE
INCLUDES=-Ibogl

COMPILE = $(CC) $(INCLUDES) $(CFLAGS)
LINK = $(CC) $(CFLAGS) $(LDFLAGS)

INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_PROGRAM = $(INSTALL) -m 755


TARGETS = usplash usplash_write

usplash_BACKEND_LDFLAGS = -ldl

ifeq ($(BACKEND), svga)
	BACKEND_dir = bogl svgalib
	usplash_BACKEND = usplash_svga.o usplash_bogl.o
	usplash_BACKEND_LIBS = svgalib/staticlib/libvgagl.a svgalib/staticlib/libvga.a bogl/libbogl.a
	usplash_BACKEND_LDFLAGS += -lx86
	CFLAGS += -DSVGA
else
	BACKEND_dir = bogl
	usplash_BACKEND = usplash_bogl.o
	usplash_BACKEND_LIBS = bogl/libbogl.a
endif

all: $(BACKEND_dir) $(TARGETS)

libusplash_OBJECTS = libusplash.o usplash-testcard.o usplash-testcard-theme.o $(usplash_BACKEND) bogl/helvB10.o
usplash_LIBS = 

usplash: libusplash.so.0 usplash.c
	$(COMPILE) -c usplash.c -o usplash.o
	$(LINK) -o $@ usplash.o -L. -lusplash

libusplash.so.0: $(libusplash_OBJECTS) $(usplash_BACKEND_LIBS)
	$(CC) -Wl,-soname=$@ -shared -o $@ $^ $(usplash_BACKEND_LDFLAGS)
	ln -s $@ libusplash.so

usplash_write_OBJECTS = usplash_write.o

usplash_write: $(usplash_write_OBJECTS)
	$(LINK) -o $@ $^


bogl:
	$(MAKE) -C bogl

svgalib:
	$(MAKE) -C svgalib

.c.o:
	$(COMPILE) -o $@ -c $<

.png.c:
	./bogl/pngtobogl $< > $@

.bdf.c:
	./bogl/bdftobogl $< > $@


install:
	$(INSTALL) -d $(DESTDIR)/sbin $(DESTDIR)/lib $(DESTDIR)/usr/include $(DESTDIR)/usr/bin
	$(INSTALL_PROGRAM) usplash $(DESTDIR)/sbin
	$(INSTALL_PROGRAM) usplash_write $(DESTDIR)/sbin
	$(INSTALL_PROGRAM) usplash_down $(DESTDIR)/sbin
	$(INSTALL_PROGRAM) libusplash.so.0 $(DESTDIR)/lib/libusplash.so.0
	ln -sf /lib/libusplash.so.0 $(DESTDIR)/lib/libusplash.so
	$(INSTALL_PROGRAM) bogl/pngtobogl $(DESTDIR)/usr/bin/pngtousplash
	$(INSTALL_PROGRAM) bogl/bdftobogl $(DESTDIR)/usr/bin/bdftousplash
	$(INSTALL_DATA) usplash-theme.h $(DESTDIR)/usr/include/usplash-theme.h
	$(INSTALL_DATA) usplash_backend.h $(DESTDIR)/usr/include/usplash_backend.h
	$(INSTALL_DATA) libusplash.h $(DESTDIR)/usr/include/libusplash.h

clean:
	-$(MAKE) -C bogl clean
	-$(MAKE) -C svgalib clean
	-rm -f $(TARGETS) $(usplash_OBJECTS) $(usplash_write_OBJECTS) libusplash.so*
	-rm -f *~	
	-rm -f *.o


.PHONY: all bogl svgalib install clean
.SUFFIXES: .png .bdf
