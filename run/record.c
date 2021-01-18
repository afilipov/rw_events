/*-
 * Copyright (c) 2019 Atanas Filipov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * [7fba731c4348b60efda90c04416302123bd4693a]
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/limits.h>

#include "common.h"

static char devices[] = "/dev/input/";
static char out_file[] = "/tmp/events.bin";

static uint8_t ev_source_count = 0;
static event_source_t *ev_source;

struct pollfd *in_fds;
FILE *ofile;

static int loop = 1;

static int acquire(void)
{
	char buffer[PATH_MAX];

	ofile = fopen(out_file, "w+");
	if(ofile < 0) {
		printf("Couldn't open output file\n");
		return -1;
	}

	in_fds = malloc(sizeof(struct pollfd) * ev_source_count);

	for(uint8_t i = 0; i < ev_source_count; i++) {
		sprintf(buffer, "%s/%s", devices, ev_source[i].ev_device_name);
		in_fds[i].events = POLLIN;
		in_fds[i].fd = open(buffer, O_RDONLY | O_NDELAY);
		if(in_fds[i].fd < 0) {
			printf("Couldn't open input device %s\n", ev_source[i].ev_device_name);
			return -1;
		}
	}

	return 0;
}

static void release(void)
{
	for (uint8_t i = 0; i < ev_source_count; i++) {
		close(in_fds[i].fd);
	}

	free(in_fds);

	fclose(ofile);

	if (ev_source != NULL && ev_source_count > 0) {
		free_event_sources(ev_source, ev_source_count);
    }
}

static void sig_handler(int signo)
{
	if (signo == SIGINT) {
		release();
        exit(EXIT_SUCCESS);
    }
}

static int record(void)
{
	struct input_event event;
    event_record_t record;
	bool skip_write = false;
	int numread;

	while(loop) {

		if(poll(in_fds, ev_source_count, -1) < 0)
			return -1;

		for(uint8_t i = 0; i < ev_source_count; i++) {
			if(in_fds[i].revents & POLLIN) {
                memset(&event, 0, sizeof(event));
				/* Data available */
				numread = read(in_fds[i].fd, &event, sizeof(event));
				if(numread != sizeof(event)) {
#ifdef DEBUG
					printf("Read error\n");
#endif
					continue;
				}

                if (event.type == EV_KEY) {
                    if (event.code == KEY_LEFTCTRL ||
                        event.code == KEY_RIGHTCTRL) {
                        if (event.value) {
                            skip_write = true;
                        } else {
                            skip_write = false;
                        }
                    }
                }

                if (!skip_write) {
                    record.ev_device_id = i;
                    record.event = event;
                    if(fwrite(&record, 1, sizeof(record), ofile) != sizeof(record)) {
                        printf("Cannot write output record\n");
                        return -1;
                    }
                }
#ifdef DEBUG
				printf("input %d, time %ld.%06ld, type %d, code %d, value %d\n",
					   i, event.time.tv_sec, event.time.tv_usec, event.type,
					   event.code, event.value);
#endif
			}
		}
	}

    return 0;
}

static void show_help(void)
{
    printf("Usage: event_record <options>\n");
    printf("Where -h print help\n");
    printf("      -d devices  : The location of Input devices,\n");
    printf("                    the default value is: %s\n", devices);
    printf("      -f out_file : The output file name.\n");
    printf("                    the default value is: %s\n", out_file);
}

int main(int argc, char **argv)
{
    int opt;

    if (argc < 2) {
        show_help();
        exit(EXIT_SUCCESS);
    }

    while ((opt = getopt(argc, argv, "h?d:o:")) != -1) {
        switch (opt) {
        case 'h':
        case '?':
            show_help();
            exit(EXIT_SUCCESS);
        case 'd':
            if(sscanf(optarg, "%s", devices) != 1) {
                printf("Can't convert input folder\n");
				exit(EXIT_FAILURE);
            }
            break;
        case 'o':
            if(sscanf(optarg, "%s", out_file) != 1) {
                printf("Can't convert output file name\n");
				exit(EXIT_FAILURE);
            }
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("Can't catch SIGINT\n");

	ev_source = alloc_event_sources(devices, &ev_source_count);

	if (acquire()) {
		printf("Acquire device failed\n");
        exit(EXIT_FAILURE);
	}

    printf("Recording use CTRL+C to stop\n");
	if (record()) {
		printf("Recording failed\n");
        exit(EXIT_FAILURE);
    }

    release();

	return EXIT_SUCCESS;
}
