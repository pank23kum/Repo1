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

#include <linux/vt.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>

#include "usplash_backend.h"
#include "usplash.h"
#include "libusplash.h"

static int main_loop(void);
static int read_command(void);
static int parse_command(const char *string, size_t len);
static void do_animate(int ignore);

/* Number of seconds to wait for a command before exiting */
static int timeout = 15;

/* Are we pulsating the throbber? */
static int pulsating = 0;
static int framedelay = 40000;
static int cycles = 0;
static int cycle_timeout = 375;

static int fifo_fd;


int main(int argc, char *argv[])
{
	int i, ret = 0;
	int xres = 0;
	int yres = 0;
	int verbose = 0;
	struct timeval t1 = {.tv_sec = 0,.tv_usec = 40000 };
	struct timeval t2 = {.tv_sec = 0,.tv_usec = 40000 };
	struct itimerval iv = {.it_interval = t1,.it_value = t2 };

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c")) {
			int vt_fd = open("/dev/tty8", O_RDWR);
			switch_console(8, vt_fd);
			close(vt_fd);
		} else if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-x")) {
			xres = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-y")) {
			yres = atoi(argv[++i]);
		} else {
			exit(1);
		}
	}

	if (chdir(USPLASH_DIR) < 0) {
		perror("chdir");
		ret = 1;
		goto exit;
	}

	if (mkfifo(USPLASH_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) <
	    0) {
		if (errno != EEXIST) {
			perror("mkfifo");
			ret = 1;
			goto exit;
		}
	}

	if (mkfifo(USPLASH_OUTFIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
	    < 0) {
		if (errno != EEXIST) {
			perror("mkfifo");
			ret = 1;
			goto exit;
		}
	}

	fifo_fd = open(USPLASH_FIFO, O_RDONLY | O_NONBLOCK);
	if (fifo_fd < 0) {
		perror("open");
		ret = 2;
		goto exit;
	}

	if (usplash_setup(xres, yres, verbose)) {
		fprintf(stderr, "screen init failed\n");
		ret = 3;
		goto exit;
	}

	signal(SIGALRM, do_animate);
	setitimer(ITIMER_REAL, &iv, NULL);

	clear_screen();
	clear_progressbar();
	clear_text();

	ret = main_loop();

	usplash_done();
      exit:
	/* While usplash is running, /dev/console is actually tty8
	 * so if we flip back to where we came from, we actually hide the
	 * very messages we're trying to show the user!
	 *
	 * The case of usplash terminating normally is already handled
	 * in the init script with an explicit chvt.
	 */
//      usplash_restore_console ();
	return ret;
}

static void do_animate(int ignore)
{
	animate_step(pulsating);
}

static int main_loop(void)
{
	struct timeval tv;

	for (;;) {
		fd_set rfds;
		int retval;

		FD_ZERO(&rfds);
		FD_SET(fifo_fd, &rfds);

		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(fifo_fd + 1, &rfds, NULL, NULL, &tv);

		if (retval < 0 && errno != EINTR) {
			/* Error */
			return 1;
		} else if (retval < 0 && errno == EINTR) {
			/* Count cycles for timeout */
			cycles++;
			if (cycles >= cycle_timeout)
				return 0;
		} else if (retval > 0) {
			/* Data available */
			int ret;
			cycles = 0;
			ret = read_command();
			if (ret)
				return ret;
		}
	}

	/* Not reached */
	return 0;
}

static int read_command(void)
{
	static char buf[4096], *ptr;
	static size_t buflen;
	static ssize_t len;

	len = read(fifo_fd, buf + buflen, sizeof(buf) - buflen);
	if (len < 0) {
		if (errno != EAGAIN) {
			/* Try opening again */
			close(fifo_fd);
			fifo_fd =
			    open(USPLASH_FIFO, O_RDONLY | O_NONBLOCK);
			if (fifo_fd < 0)
				return 2;
		}

		return 0;
	} else if (len == 0) {
		/* Reopen to see if there's anything more for us */
		close(fifo_fd);
		fifo_fd = open(USPLASH_FIFO, O_RDONLY | O_NONBLOCK);
		if (fifo_fd < 0)
			return 2;

		return 0;
	}

	buflen += len;
	while ((ptr = memchr(buf, '\0', buflen)) != NULL) {
		int ret;

		ret = parse_command(buf, ptr - buf);
		if (ret)
			return ret;

		/* Move the rest of the buffer up */
		buflen -= ptr - buf + 1;
		memcpy(buf, ptr + 1, buflen);
	}

	return 0;
}

static int parse_command(const char *string, size_t len)
{
	const char *command;
	size_t commandlen;

	command = string;
	commandlen = strncspn(string, len, " ");

	if (string[commandlen] == ' ') {
		string += commandlen + 1;
		len -= commandlen + 1;
	} else {
		len = 0;
	}


	if (!strncmp(command, "QUIT", commandlen)) {
		return 1;

	} else if (!strncmp(command, "TIMEOUT", commandlen)) {
		timeout = atoi(string);
		cycle_timeout = (1000000 / framedelay) * timeout;

	} else if (!strncmp(command, "CLEAR", commandlen)) {
		clear_text();

	} else if (!strncmp(command, "TEXT", commandlen)) {
		draw_text(string, len);

	} else if (!strncmp(command, "TEXT-URGENT", commandlen)) {
		draw_text_urgent(string, len);

	} else if (!strncmp(command, "STATUS", commandlen)) {
		draw_status(string, len, 0);

	} else if (!strncmp(command, "SUCCESS", commandlen)) {
		draw_status(string, len, 1);

	} else if (!strncmp(command, "FAILURE", commandlen)) {
		draw_status(string, len, -1);

	} else if (!strncmp(command, "PROGRESS", commandlen)) {
		pulsating = 0;
		draw_progressbar(atoi(string));

	} else if (!strncmp(command, "PULSATE", commandlen)) {
		pulsating = 1;

	} else if (!strncmp(command, "INPUT", commandlen)) {
		return handle_input(string, len, 0);

	} else if (!strncmp(command, "INPUTQUIET", commandlen)) {
		return handle_input(string, len, 1);

	} else if (!strncmp(command, "INPUTENTER", commandlen)) {
		return handle_input(string, len, 2);
	}

	return 0;
}
