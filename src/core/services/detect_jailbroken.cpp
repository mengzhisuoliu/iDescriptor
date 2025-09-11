#include "../../iDescriptor.h"
#include <libimobiledevice/afc.h>
// char *possible_jailbreak_paths[] = {
//     "/Applications/Cydia.app",
//     "/Library/MobileSubstrate/MobileSubstrate.dylib",
//     "/bin/bash",
//     "/usr/sbin/sshd",
//     "/etc/apt",
//     NULL
// };
#include <string>

bool detect_jailbroken(afc_client_t afc)
{
    char **dirs = NULL;
    if (afc_read_directory(afc, (std::string(POSSIBLE_ROOT) + "bin").c_str(),
                           &dirs) == AFC_E_SUCCESS) {
        // if we can loop through the directory, it means we have access to the
        // file system
        for (char **dir = dirs; *dir != nullptr; ++dir) {
            afc_dictionary_free(dirs);
            return true;
        }
    }
    afc_dictionary_free(dirs);
    return false;
}