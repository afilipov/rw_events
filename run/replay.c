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
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/limits.h>

#include "common.h"

static char devices[] = "/dev/input";
static char in_file[] = "/tmp/events.bin";

static uint8_t ev_source_count = 0;
static event_source_t *ev_source;

static int *out_fds = NULL;

FILE *ifile;

static int loop = 1;

static int acquire(void)
{
	char buf[PATH_MAX];

	out_fds = malloc(sizeof(int) * ev_source_count);

	for(uint8_t i = 0; i < ev_source_count; i++) {
		sprintf(buf, "%s/%s", devices, ev_source[i].ev_device_name);
		out_fds[i] = open(buf, O_WRONLY);
		if (out_fds[i] < 0) {
			printf("Couldn't open output device\n");
			return -1;
		}
	}

    ifile = fopen(in_file, "r");
    if (!ifile) {
		printf("Couldn't open input\n");
        return -1;
    }

	return 0;
}

static void release(void)
{
	for(uint8_t i = 0; i < ev_source_count; i++) {
		close(out_fds[i]);
	}

	free(out_fds);
	fclose(ifile);

	if (ev_source != NULL && ev_source_count > 0) {
		free_event_sources(ev_source, ev_source_count);
    }
}

static void sig_handler(int signo)
{
	if (signo == SIGINT) {
		loop = 0;
        release();
    }
}

static int replay(void)
{
    event_record_t record;
	struct timeval tdiff;
    struct timeval now, tevent, tsleep;

	timerclear(&tdiff);

    while (loop) {

        if (fread(&record, 1, sizeof(record), ifile) != sizeof(record)) {
            if (feof(ifile)) {
                return 0;
            }

            return -1;
        }

		gettimeofday(&now, NULL);
		if (!timerisset(&tdiff)) {
			timersub(&now, &record.event.time, &tdiff);
		}

		timeradd(&record.event.time, &tdiff, &tevent);
		timersub(&tevent, &now, &tsleep);
		if (tsleep.tv_sec || tsleep.tv_usec) {
			select(0, NULL, NULL, NULL, &tsleep);
        }

		record.event.time = tevent;

        if(write(out_fds[record.ev_device_id], &record.event,
                 sizeof(record.event)) != sizeof(record.event)) {
            printf("Output write error\n");
            return -1;
        }
#ifdef DEBUG
		printf("input %d, time %ld.%06ld, type %d, code %d, value %d\n",
               record.ev_device_id, record.event.time.tv_sec,
               record.event.time.tv_usec, record.event.type,
               record.event.code, record.event.value);
#endif
    };

	return 0;
}

static void show_help(void)
{
    printf("Usage: event_record <options>\n");
    printf("Where -h print help\n");
    printf("      -d devices : The location of Input devices,\n");
    printf("                   the default value is: %s\n", devices);
    printf("      -f in_file : The input file name.\n");
    printf("                   the default value is: %s\n", in_file);
}

int main(int argc, char **argv)
{
    int opt;

    if (argc < 2) {
        show_help();
        exit(EXIT_SUCCESS);
    }

    while ((opt = getopt(argc, argv, "h?d:f:")) != -1) {
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
        case 'f':
            if(sscanf(optarg, "%s", in_file) != 1) {
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

	if(acquire()) {
		printf("Acquire devices failed\n");
        exit(EXIT_FAILURE);
	}

	if(replay()) {
		printf("Replay failed\n");
        exit(EXIT_FAILURE);
	}

    release();

	return EXIT_SUCCESS;
}
