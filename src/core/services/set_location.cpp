/*
 * idevicesetlocation.c
 * Simulate location on iOS device with mounted developer disk image
 *
 * Copyright (c) 2016-2020 Nikias Bassen, All Rights Reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "../../iDescriptor.h"
#define DT_SIMULATELOCATION_SERVICE "com.apple.dt.simulatelocation"

#include <errno.h>
#include <getopt.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/service.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#include <machine/endian.h>
#define htobe32(x) OSSwapHostToBigInt32(x)
#else
#include <endian.h>
#endif
#include <QDebug>

// TODO: check for these
//  if (device_version >= IDEVICE_DEVICE_VERSION(17,0,0)) {
//  		printf("Note: This tool is currently not supported on iOS 17+\n");
//  	} else {
//  		printf("Make sure a developer disk image is mounted!\n");
//  	}

enum { SET_LOCATION = 0, RESET_LOCATION = 1 };
bool set_location(idevice_t device, char *lat, char *lon)
{
    uint32_t mode = 0;
    lockdownd_client_t lockdown = NULL;
    lockdownd_error_t lerr =
        lockdownd_client_new_with_handshake(device, &lockdown, TOOL_NAME);
    try {
        /* code */

        if (lerr != LOCKDOWN_E_SUCCESS) {
            idevice_free(device);
            printf("ERROR: Could not connect to lockdownd: %s (%d)\n",
                   lockdownd_strerror(lerr), lerr);
            return false;
        }

        lockdownd_service_descriptor_t svc = NULL;
        lerr = lockdownd_start_service(lockdown, DT_SIMULATELOCATION_SERVICE,
                                       &svc);
        if (lerr != LOCKDOWN_E_SUCCESS) {
            unsigned int device_version = idevice_get_device_version(device);
            lockdownd_client_free(lockdown);
            idevice_free(device);

            return false;
        }
        lockdownd_client_free(lockdown);

        service_client_t service = NULL;

        service_error_t serr = service_client_new(device, svc, &service);

        lockdownd_service_descriptor_free(svc);

        if (serr != SERVICE_E_SUCCESS) {
            idevice_free(device);
            return false;
        }

        uint32_t l;
        uint32_t s = 0;

        l = htobe32(mode);
        service_send(service, (const char *)&l, 4, &s);
        if (mode == SET_LOCATION) {
            int len = 4 + strlen(lat) + 4 + strlen(lon);
            char *buf = (char *)malloc(len);
            uint32_t latlen;
            latlen = strlen(lat);
            l = htobe32(latlen);
            memcpy(buf, &l, 4);
            memcpy(buf + 4, lat, latlen);
            uint32_t longlen = strlen(lon);
            l = htobe32(longlen);
            memcpy(buf + 4 + latlen, &l, 4);
            memcpy(buf + 4 + latlen + 4, lon, longlen);

            s = 0;
            service_send(service, buf, len, &s);
            free(buf); // <-- free the buffer after use
        }

        return true;
    } catch (...) {
        if (lockdown) {
            lockdownd_client_free(lockdown);
        }
        if (device) {
            idevice_free(device);
        }
        qDebug() << "Exception occurred while setting location.";
        return false;
    }
}