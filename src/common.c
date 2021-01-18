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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/uinput.h>
#include "common.h"

#define MOVE_LOOPS 4

static const event_record_t to_lower_left[] = {
    {
        .ev_device_id   =  10,
        .event.type     =   2,
        .event.code     =   0,
        .event.value    = -1024
    },
    {
        .ev_device_id   =  10,
        .event.type     =   2,
        .event.code     =   1,
        .event.value    = +1024
    },
    {
        .ev_device_id   =  10,
        .event.type     =   0,
        .event.code     =   0,
        .event.value    =   0
    },
};

event_source_t* alloc_event_sources(const char *path, uint8_t* num_sources)
{
    DIR *d;
    struct dirent *dir;
    static event_source_t *sources;
    uint8_t count = 0;

    d = opendir(path);
    if (d == NULL) {
        return NULL;
    }

    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type != DT_DIR) {
            sources = realloc(sources, sizeof(event_source_t) * (count + 1));
            sources[count].ev_device_id = count;
            sources[count].ev_device_name = strdup(dir->d_name);
            count++;
        }
    }

    closedir(d);

    if (!count) {
        return NULL;
    }

    *num_sources = count;

    return sources;
}

void free_event_sources(event_source_t *ev_source, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        if (ev_source != NULL && ev_source[i].ev_device_name != NULL) {
            free(ev_source[i].ev_device_name);
        }
    }
}

int acquire_uinput(int fd)
{
    struct uinput_user_dev uidev;

    // Enable Mouse events simulation
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        printf("Cannot enable button events\n");
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0) {
        printf("Can't setup button event source\n");
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_WHEEL) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_GEAR_DOWN) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, BTN_GEAR_UP) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) {
        printf("Can't enable relative events\n");
        return -1;
    }

    if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0) {
        printf("Cannot setup event sources %d\n", __LINE__);
        return -1;
    }

    if (ioctl(fd, UI_SET_RELBIT, REL_Y) < 0) {
        printf("Can't setup event sources %d\n", __LINE__);
        return -1;
    }

    // Enable Keyboard events simulation
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        printf("Can't enable key events\n");
        return -1;
    }

    for (uint16_t i = KEY_ESC; i < KEY_MAX; i++) {
        if (ioctl(fd, UI_SET_KEYBIT, i) < 0) {
            printf("Can't setup event sources %d\n", __LINE__);
            return -1;
        }
    }

    // Enable Touch events simulation
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        printf("Cannot enable touch events\n");
        return -1;
    }

    for (uint16_t i = ABS_X; i < ABS_MAX; i++) {
        if (ioctl(fd, UI_SET_ABSBIT, i) < 0) {
            printf("Can't setup event sources %d\n", __LINE__);
            return -1;
        }
    }

    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-LVRG");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x03eb;  // Lope de Vega Research Group :)
    uidev.id.product = 0x6200;  // HID Device emulator
    uidev.id.version = 1;

    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        printf("Write to device failed\n");
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        printf("IOC Device create failed\n");
        return -1;
    }

    return 0;
}

int release_uinput(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
        printf("IOC Device destroy failed\n");
        return -1;
    }

    close(fd);

    return 0;
}

int set_position(int fd)
{
    // Move mouse on base position
    for (int i = 0; i < MOVE_LOOPS; i++) {
        for (unsigned int s = 0; s < sizeof(to_lower_left) / sizeof(to_lower_left[0]); s++) {
            if (write(fd, &to_lower_left[s].event, sizeof(to_lower_left[s].event)) !=
                      sizeof(to_lower_left[s].event)) {
                return -1;
            }
        }
    }

    return 0;
}
