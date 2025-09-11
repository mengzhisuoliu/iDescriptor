#include "../../iDescriptor.h"
#include <stdlib.h>
#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#ifndef _WIN32
#include <signal.h>
#endif

#include <QDebug>
#include <libimobiledevice-glue/sha.h>
#include <libimobiledevice-glue/utils.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/mobile_image_mounter.h>
#include <libimobiledevice/notification_proxy.h>
#include <libtatsu/tss.h>
#include <plist/plist.h>
#include <printf.h>

QPair<bool, plist_t> _get_mounted_image(const char *udid)
{
    mobile_image_mounter_client_t mim = NULL;
    int res = -1;
    lockdownd_client_t lckd = NULL;
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    afc_client_t afc = NULL;
    lockdownd_service_descriptor_t service = NULL;
    idevice_t device = NULL;

    mobile_image_mounter_error_t err = MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
    plist_t result = NULL;
    size_t sig_length = 0;
    char *imagetype = "Developer";

    if (IDEVICE_E_SUCCESS != idevice_new_with_options(&device, udid,

                                                      IDEVICE_LOOKUP_USBMUX)) {
        qDebug() << "ERROR: Could not create idevice!";
        res = -1;
        goto leave;
    }

    if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
                                   device, &lckd, TOOL_NAME))) {
        qDebug() << "ERROR: Could not connect to lockdownd service!";
        res = -1;
        goto leave;
    }

    lockdownd_start_service(lckd, "com.apple.mobile.mobile_image_mounter",
                            &service);

    if (!service || service->port == 0) {
        printf("ERROR: Could not start mobile_image_mounter service!\n");
        goto leave;
    }

    if (mobile_image_mounter_new(device, service, &mim) !=
        MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        printf("ERROR: Could not connect to mobile_image_mounter!\n");
        goto leave;
    }

    if (!service || service->port == 0) {
        qDebug() << "ERROR: Could not start mobile_image_mounter service!";
        res = -1;
        goto leave;
    }

    // if locked
    //     {
    //   "Error": "DeviceLocked"
    // }
    err = mobile_image_mounter_lookup_image(mim, imagetype, &result);
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        res = 0;
    } else {
        res = -1;
        printf("Error: lookup_image returned %d\n", err);
    }

leave:
    // if (f) {
    //     fclose(f);
    // }
    // TODO:need to free result
    // if (result) {
    //     plist_free(result);
    // }
    if (mim) {
        mobile_image_mounter_free(mim);
    }
    if (afc) {
        afc_client_free(afc);
    }
    if (lckd) {
        lockdownd_client_free(lckd);
    }
    if (device) {
        idevice_free(device);
    }

    return {res == 0, result};
}

// int main(){return 0;}