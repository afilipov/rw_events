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
 */

#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/limits.h>

#include "common.h"

static const char *in_records = "/tmp/events.bin";
static const char *uinput_node = "/dev/uinput";

static bool show_info = false;
static bool loop = true;

static void sig_handler(int signo)
{
    if (signo == SIGINT) {
        loop = 0;
    }
}

static int replay(int fd, FILE *in_file)
{
    event_record_t record;
    struct timeval tdiff;
    struct timeval now, tevent, tsleep;

    timerclear(&tdiff);

    while (loop) {

        if (fread(&record, 1, sizeof(record), in_file) != sizeof(record)) {
            if (feof(in_file)) {
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
        if (write(fd, &record.event, sizeof(record.event)) != sizeof(record.event)) {
            ON_ERROR("Can't propagate event");
        }

        if (show_info) {
            printf("input %d, time %ld.%06ld, type %d, code %d, value %d\n",
                   record.ev_device_id, record.event.time.tv_sec,
                   record.event.time.tv_usec, record.event.type,
                   record.event.code, record.event.value);
        }
    };

    return 0;
}

static void show_help(void)
{
    printf("Usage: ev_replay <options>\n");
    printf("Where -h print help\n");
    printf("      -f input : The input file name\n");
    printf("                   the default value is: %s\n", in_records);
    printf("      -n       : Skip mouse position setup\n");
    printf("                   the default value is false\n");
    printf("      -v       : Verbose output\n");
    printf("                   the default value is false\n");
}

int main(int argc, char **argv)
{
    int opt;
    int fd;
    FILE *in_file;
    bool move_to = true;

    while ((opt = getopt(argc, argv, "h?nvf:")) != -1) {
        switch (opt) {
            case 'h':
            case '?':
                show_help();
                exit(EXIT_SUCCESS);
            case 'f':
                in_records = optarg;
                break;
            case 'n':
                move_to = false;
                break;
            case 'v':
                show_info = true;
                break;
            default:
                show_help();
                exit(EXIT_SUCCESS);
        }
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        ON_ERROR("Can't catch SIGINT");
    }

    in_file = fopen(in_records, "r");
    if (!in_file) {
        ON_ERROR("Can't open input file");
    }

    fd = open(uinput_node, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        ON_ERROR("Open uinput device failed");
    }

    if (acquire_uinput(fd)) {
        ON_ERROR("Acquire output devices failed");
    }

    // Move mouse on base position
    if (move_to) {
        sleep(1);
        if (set_position(fd)) {
            ON_ERROR("Can't setup cursor position");
        }
    }

    if (replay(fd, in_file)) {
        ON_ERROR("Records replay failed");
    }

    // Move mouse on base position
    if (move_to) {
        if (set_position(fd)) {
            ON_ERROR("Can't setup cursor position");
        }
    }

    if (release_uinput(fd)) {
        ON_ERROR("Release output device failed");
    }

    close(fd);

    return EXIT_SUCCESS;
}
