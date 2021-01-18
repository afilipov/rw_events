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

static const char *in_folder = "/dev/input";
static const char *out_fname = "/tmp/events.bin";
static const char *uinput_node = "/dev/uinput";

static bool show_info = false;
static bool loop = true;
static event_source_t *ev_source;

static int prepare(int ufd)
{
    if (acquire_uinput(ufd)) {
        printf("Acquire output devices failed\n");
        return -1;
    }

    sleep(1);

    if (set_position(ufd)) {
        printf("Can't setup cursor position\n");
        return -1;
    }

    if (release_uinput(ufd)) {
        printf("Release output device failed\n");
        return -1;
    }

    return 0;
}

static int acquire(struct pollfd *fds, uint8_t count)
{
    char buffer[PATH_MAX];

    for(uint8_t i = 0; i < count; i++) {
        sprintf(buffer, "%s/%s", in_folder, ev_source[i].ev_device_name);
        fds[i].events = POLLIN;
        fds[i].fd = open(buffer, O_RDONLY | O_NDELAY);
        if(fds[i].fd < 0) {
            printf("Can't open input device node %s\n", ev_source[i].ev_device_name);
            return -1;
        }
    }

    return 0;
}

static int release(struct pollfd *fds, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        if (close(fds[i].fd)) {
            printf("Can't close input device node %s\n", ev_source[i].ev_device_name);
            return -1;
        }
    }

    if (ev_source != NULL && count > 0) {
        free_event_sources(ev_source, count);
    }

    return 0;
}

static void sig_handler(int signo)
{
    if (signo == SIGINT) {
        loop = false;
    }
}

static int record(struct pollfd *fds, uint8_t count, FILE *ohandle)
{
    event_record_t record;
    bool skip_write = false;
    int size;

    while(loop) {

        if(poll(fds, count, -1) < 0) {
            return -1;
        }

        for(uint8_t i = 0; i < count; i++) {
            if(fds[i].revents & POLLIN) {
                memset(&record, 0, sizeof(record));
                // Data available
                size = read(fds[i].fd, &record.event, sizeof(record.event));
                if(size != sizeof(record.event)) {
#ifdef DEBUG
                    printf("Unexpected event size %d\n", size);
#endif
                    continue;
                }

                // If CTRL + key pressed, wait until CTRL key released
                if (record.event.type == EV_KEY) {
                    if (record.event.code == KEY_LEFTCTRL || record.event.code == KEY_RIGHTCTRL) {
                        if (record.event.value) {
                            skip_write = true;
                        } else {
                            skip_write = false;
                        }
                    }
                }

                if (!skip_write) {
                    record.ev_device_id = i;
                    if(fwrite(&record, 1, sizeof(record), ohandle) != sizeof(record)) {
                        printf("Cannot write output record\n");
                        return -1;
                    }
                }

                if (show_info) {
                    printf("input %d, time %ld.%06ld, type %d, code %d, value %d\n",
                           i, record.event.time.tv_sec, record.event.time.tv_usec,
                           record.event.type, record.event.code, record.event.value);
                }
            }
        }
    }

    return 0;
}

static void show_help(void)
{
    printf("Usage: ev_record <options>\n");
    printf("Where -h print help\n");
    printf("      -d inputs : The location of input nodes\n");
    printf("                    the default value is: %s\n", in_folder);
    printf("      -f output : The output file name\n");
    printf("                    the default value is: %s\n", out_fname);
    printf("      -n        : Skip mouse position setup\n");
    printf("                    the default value is false\n");
    printf("      -v        : Verbose output\n");
    printf("                    the default value is false\n");
}

int main(int argc, char **argv)
{
    int opt;
    FILE *out_hdl;
    struct pollfd *in_fds = NULL;
    static uint8_t num_nodes = 0;
    bool move_to = true;
    int ufd;

    while ((opt = getopt(argc, argv, "h?vnd:f:")) != -1) {
        switch (opt) {
        case 'h':
        case '?':
            show_help();
            exit(EXIT_SUCCESS);
        case 'd':
            in_folder = optarg;
            break;
        case 'f':
            out_fname = optarg;
            break;
        case 'n':
            move_to = false;
            break;
        case 'v':
            show_info = true;
            break;
        default:
            show_help();
            ON_ERROR("Unknown option");
        }
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        ON_ERROR("Can't catch SIGINT");
    }

    // Move the mouse cursor to lower left corner before start of recording
    if (move_to) {
        ufd = open(uinput_node, O_WRONLY | O_NONBLOCK);
        if (ufd < 0) {
            ON_ERROR("Open uinput device failed");
        }

        if (prepare(ufd)) {
            ON_ERROR("Can't setup base position");
        }
        close(ufd);
    }

    out_hdl = fopen(out_fname, "w");
    if (!out_hdl) {
        ON_ERROR("Can't create output file");
    }

    ev_source = alloc_event_sources(in_folder, &num_nodes);
    if (!ev_source) {
            ON_ERROR("Can't allocate event nodes");
    }

    in_fds = malloc(sizeof(struct pollfd) * num_nodes);
    if (!in_fds) {
        ON_ERROR("Can't allocate resources");
    }

    if (acquire(in_fds, num_nodes)) {
        ON_ERROR("Acquire input devices failed");
    }

    printf("Recording started, use CTRL+C to stop it\n");
    if (record(in_fds, num_nodes, out_hdl)) {
        if (errno != EINTR) {
            ON_ERROR("Recording failed");
        }
    }

    if (release(in_fds, num_nodes)) {
        ON_ERROR("Resources release failed");
    }

    free(in_fds);

    if (fclose(out_hdl)) {
        ON_ERROR("Can't close output file");
    }

    return EXIT_SUCCESS;
}
