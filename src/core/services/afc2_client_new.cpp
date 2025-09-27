#include "../../iDescriptor.h"
#include <QDebug>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

afc_error_t afc2_client_new(idevice_t device, afc_client_t *afc)
{

    lockdownd_service_descriptor_t service = NULL;
    // TODO: should free service ?
    lockdownd_client_t client = NULL;

    if (lockdownd_client_new_with_handshake(device, &client, APP_LABEL) !=
        LOCKDOWN_E_SUCCESS) {
        qDebug() << "Could not connect to lockdownd";
        return AFC_E_UNKNOWN_ERROR;
    }
    if (lockdownd_start_service(client, AFC2_SERVICE_NAME, &service) !=
        LOCKDOWN_E_SUCCESS) {
        qDebug() << "Could not start AFC service";
        lockdownd_client_free(client);
        return AFC_E_UNKNOWN_ERROR;
    }

    return afc_client_new(device, service, afc);

    // char **dirs = NULL;
    // if (afc_read_directory(afc, argv[1], &dirs) == AFC_E_SUCCESS) {
    //     for (int i = 0; dirs[i]; i++) {
    //         printf("Entry: %s\n", dirs[i]);
    //     }
    //     // free(dirs);
    // }
}