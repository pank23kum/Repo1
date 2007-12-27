/* usplash
 *
 * Copyright © 2006 Canonical Ltd.
 * Copyright © 2006 Dennis Kaarsemaker <dennis@kaarsemaker.net>
 * Copyright © 2005 Matthew Garrett <mjg59@srcf.ucam.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "usplash_backend.h"
#include "usplash_svga_backend.h"
#include "svgalib/gl/vgagl.h"
#include "svgalib/src/vga.h"
#include "usplash-theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>

extern sigset_t sigs;
#define blocksig() do{ sigprocmask(SIG_BLOCK, &sigs, NULL); } while(0)

/* 640x480 */
static int svgalib_mode = 10;
static int bpp = 8;
static int modenum;

struct vesa_mode {
	int x;
	int y;
	int mode;
};

struct palette_entry {
	int red;
	int green;
	int blue;
};

struct palette_entry *vesa_palette;

static void *initial_palette;

/* Table of 256-colour modes for now  - get from /usr/include/vga.h */

static struct vesa_mode vesa_modes[] = {
	{640, 480, 10},
	{800, 600, 11},
	{1024, 768, 12},
	{1152, 864, 39},
	{1280, 1024, 13},
	{1600, 1200, 45},
	{1920, 1440, 70},
	{-1, -1, -1},
};

extern struct usplash_font font_helvB10;
static struct usplash_font *myfont = &font_helvB10;

int usplash_svga_setfont(void *font)
{
	myfont = (struct usplash_font *) font;
	return 0;
}

static void svga_get_fontstats(char c, int *width, int *index)
{
	int offset;

	if (!((c) & myfont->index_mask))
		offset = myfont->offset[myfont->default_char];
	else
		offset = myfont->offset[(int) c];

	if (width)
		*width = myfont->index[offset];

	if (index)
		*index = myfont->index[offset + 1];
}

int usplash_svga_getfontwidth(char c)
{
	int width;

	svga_get_fontstats(c, &width, NULL);
	return width;
}

int usplash_svga_setmode() {
	int mode;
	int has_failed = 0;

	while (modenum != -1) {
		mode = vga_setmode(svgalib_mode);

		if (mode >= 0) {
			/* Success */
			if (has_failed) {
				fprintf(stderr,
					"usplash: Using mode %dx%d\n",
					vesa_modes[modenum].x,
					vesa_modes[modenum].y);
			}

			if (mode != svgalib_mode)
				bpp = 16;
			else
				bpp = 8;
			return mode;
		}		
		fprintf(stderr,
			"usplash: Setting mode %dx%d failed\n",
			vesa_modes[modenum].x, vesa_modes[modenum].y);
		has_failed = 1;
		svgalib_mode = vesa_modes[--modenum].mode;
	}
	return -1;
}

int usplash_svga_init()
{
	int rc = 0;
	int mode;

	vga_setchipset(VESA);

	usplash_save_font();

	rc = vga_init();

	if (rc) {
		return rc;
	}

	rc = usplash_svga_setmode();
	
	if (rc < 0)
		return rc;
	
	initial_palette = malloc(sizeof(char) * 768);
	gl_getpalette(initial_palette);

	return gl_setcontextvga(rc);
}

int usplash_svga_set_resolution(int x, int y)
{
	int i;

	modenum = 0;

	for (i = 0; vesa_modes[i].y != -1; i++) {
		if (vesa_modes[i].y > y)
			break;
		svgalib_mode = vesa_modes[i].mode;
		modenum = i;
	}
	return 0;
}

void usplash_svga_clear(int x1, int y1, int x2, int y2, int colour)
{
	gl_fillbox(x1, y1, x2 - x1, y2 - y1, colour);
}

void usplash_svga_move(int sx, int sy, int dx, int dy, int w, int h)
{
	gl_copybox(sx, sy, w, h, dx, dy);
}

void usplash_svga_text(int x, int y, const char *s, int len, int fg,
		       int bg)
{
	int i, j, index, width, c;

	while (len) {
		svga_get_fontstats(*s, &width, &index);

		/* Clear box */
		gl_fillbox(x, y, width, myfont->height, bg);
		/* Plot font FIXME theoretically it has to support characters widr than
		 * 32pixels */
		for (i = 0; i < myfont->height; i++) {
			c = myfont->content[index + i];
			/* Plot */
			for (j = 0; j < width; j++) {
				if (c & (1 << (32 - j)))
                                        if (bpp == 8)
                                                gl_setpixel(x + j, y + i, fg);
                                        else
                                                gl_setpixelrgb(x + j, y + i, vesa_palette[fg].red, vesa_palette[fg].green, vesa_palette[fg].blue);
			}
		}
		x += width;
		len--;
		s++;
	}
}

void usplash_svga_put(int x, int y, void *pointer)
{
	struct usplash_pixmap *pixmap = pointer;
	int size = pixmap->height * pixmap->width;

	if (bpp == 16) {	/* ZOMG pain */
		uint16_t *new_pixmap = malloc(size * sizeof(uint16_t));
		int i;
		for (i = 0; i < size; i++) {
			uint16_t value;
			value = vesa_palette[pixmap->data[i]].blue >> 3;
			value |=
			    (vesa_palette[pixmap->data[i]].
			     green >> 2) << 5;
			value |=
			    (vesa_palette[pixmap->data[i]].red >> 3) << 11;
			new_pixmap[i] = value;
		}
		gl_putbox(x, y, pixmap->width, pixmap->height, new_pixmap);
		free(new_pixmap);
	} else
		gl_putbox(x, y, pixmap->width, pixmap->height,
			  pixmap->data);
}

void usplash_svga_put_part(int x, int y, int width, int height,
			   void *pointer, int x0, int y0)
{
	int i;
	int size = width * height;
	struct usplash_pixmap *pixmap = pointer;
	unsigned char *part = (char *) malloc(size * sizeof(char));
	char *start = pixmap->data + y0 * pixmap->width + x0;
	for (i = 0; i < height; i++) {
		memcpy(part + i * width, start + i * pixmap->width, width);
	}

	if (bpp == 16) {	/* ZOMG pain */
		uint16_t *new_pixmap = malloc(size * sizeof(uint16_t));
		for (i = 0; i < size; i++) {
			uint16_t value;
			value = vesa_palette[(int) part[i]].blue >> 3;
			value |=
			    (vesa_palette[(int) part[i]].green >> 2) << 5;
			value |=
			    (vesa_palette[(int) part[i]].red >> 3) << 11;
			new_pixmap[i] = value;
		}
		gl_putbox(x, y, width, height, new_pixmap);
		free(new_pixmap);
	} else
		gl_putbox(x, y, width, height, part);

	free(part);
}

void usplash_svga_done()
{
	/* svgalib registers an at_exit handler that deals with mode 
	   restoration */
	gl_setpalette(initial_palette);
	blocksig();
	return;
}

void usplash_svga_set_palette(int ncols, unsigned char palette[][3])
{
	int i;

	if (vesa_palette)
		free(vesa_palette);

	vesa_palette = malloc(ncols * sizeof(struct palette_entry));

	for (i = 0; i < ncols; i++) {
		vesa_palette[i].red = palette[i][0];
		vesa_palette[i].green = palette[i][1];
		vesa_palette[i].blue = palette[i][2];
		palette[i][0] = palette[i][0] / 4;
		palette[i][1] = palette[i][1] / 4;
		palette[i][2] = palette[i][2] / 4;
	}
	if (bpp == 8)
	  gl_setpalettecolors(0, ncols, palette);
}

void usplash_svga_getdimensions(int *x, int *y)
{
	*x = WIDTH;
	*y = HEIGHT;
}

/* We don't need these. But svgalib wants them for linking purposes. MATTHEW
   SMASH */

double pow(double x, double y)
{
	return 0;
}

double log(double x)
{
	return 0;
}
