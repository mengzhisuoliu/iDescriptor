#include "../../iDescriptor.h"
#include <libimobiledevice/afc.h>
#include <string>
#include <vector>

struct JailbreakDetectionResult {
    bool is_jailbroken;
    std::vector<std::string> found_folders;
};

JailbreakDetectionResult detect_has_jailbroken_before(afc_client_t afc)
{
    std::vector<std::string> jailbreak_folders = {".installed_palera1n",
                                                  ".procursus_strapped"};

    JailbreakDetectionResult result = {false, {}};

    char **dirs = NULL;
    if (afc_read_directory(afc, POSSIBLE_ROOT, &dirs) == AFC_E_SUCCESS) {
        for (char **dir = dirs; *dir != nullptr; ++dir) {
            std::string dirname = *dir;
            for (const auto &jb_folder : jailbreak_folders) {
                if (dirname == jb_folder) {
                    result.found_folders.push_back(jb_folder);
                    result.is_jailbroken = true;
                }
            }
        }
    }
    afc_dictionary_free(dirs);
    return result;
}