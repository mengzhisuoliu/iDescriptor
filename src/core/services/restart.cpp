/*
 * idevicediagnostics.c
 * Retrieves diagnostics information from device
 *
 * Copyright (c) 2012 Martin Szulecki All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "../../iDescriptor.h"
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

// TODO:break all the client because device wont restart if any client is still
// connected we need to change the main device init function to not connect to
// any client
bool restart(idevice_t device)
{
    lockdownd_client_t lockdown_client = NULL;
    diagnostics_relay_client_t diagnostics_client = NULL;
    lockdownd_error_t ret = LOCKDOWN_E_UNKNOWN_ERROR;
    lockdownd_service_descriptor_t service = NULL;
    const char *udid = NULL;
    int use_network = 0;

    if (LOCKDOWN_E_SUCCESS != (ret = lockdownd_client_new_with_handshake(
                                   device, &lockdown_client, TOOL_NAME))) {
        idevice_free(device);
        printf("ERROR: Could not connect to lockdownd, error code %d\n", ret);
        return false;
    }

    /*  attempt to use newer diagnostics service available on iOS 5 and later */
    ret = lockdownd_start_service(
        lockdown_client, "com.apple.mobile.diagnostics_relay", &service);
    if (ret == LOCKDOWN_E_INVALID_SERVICE) {
        /*  attempt to use older diagnostics service */
        ret = lockdownd_start_service(
            lockdown_client, "com.apple.iosdiagnostics.relay", &service);
    }
    lockdownd_client_free(lockdown_client);

    if (ret != LOCKDOWN_E_SUCCESS) {
        idevice_free(device);
        printf("ERROR: Could not start diagnostics relay service: %s\n",
               lockdownd_strerror(ret));
        return false;
    }

    if ((ret == LOCKDOWN_E_SUCCESS) && service && (service->port > 0)) {
        if (diagnostics_relay_client_new(device, service,
                                         &diagnostics_client) !=
            DIAGNOSTICS_RELAY_E_SUCCESS) {
            printf("ERROR: Could not connect to diagnostics_relay!\n");
        } else {

            if (diagnostics_relay_restart(
                    diagnostics_client,
                    DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) ==
                DIAGNOSTICS_RELAY_E_SUCCESS) {
                printf("Restarting device.\n");
                return true;
            } else {
                printf("ERROR: Failed to restart device.\n");
            }
        }

        diagnostics_relay_goodbye(diagnostics_client);
        diagnostics_relay_client_free(diagnostics_client);
    }

    if (service) {
        lockdownd_service_descriptor_free(service);
        service = NULL;
    }

    return false;
}