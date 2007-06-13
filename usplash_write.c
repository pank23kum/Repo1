/* usplash
 *
 * usplash_write.c - send commands to usplash
 *
 * Copyright © 2006 Canonical Ltd.
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usplash.h"


int main(int argc, char *argv[])
{
	int i, fifo_fd;

	if (argc < 2) {
		fprintf(stderr, "Usage: usplash_write COMMAND...\n");
		exit(1);
	}

	if (chdir(USPLASH_DIR) < 0) {
		if (errno != ENOENT)
			perror("chdir");
		exit(0);
	}

	fifo_fd = open(USPLASH_FIFO, O_WRONLY | O_NONBLOCK);
	if (fifo_fd < 0) {
		if ((errno != ENXIO) && (errno != ENOENT))
			perror("open");
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		ssize_t len;

		len = write(fifo_fd, argv[i], strlen(argv[i]) + 1);
		if (len < 0) {
			perror("write");
			exit(0);
		}
	}

	return 0;
}
