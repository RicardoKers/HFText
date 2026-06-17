#include "hftext_c_api.h"

#include <cstring>
#include <iostream>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    if (!require(std::strcmp(hftext_c_application_name(), "HFText") == 0, "Unexpected application name")) {
        return 1;
    }
    if (!require(std::strlen(hftext_c_version()) > 0, "Version string is empty")) {
        return 1;
    }

    HFTextAppModemProfiles profiles{};
    if (!require(
            hftext_c_default_app_modem_profiles(&profiles) == HFTEXT_STATUS_OK,
            "Could not load default modem profiles"
        )) {
        return 1;
    }
    if (!require(
            profiles.slow.modulation_mode == HFTEXT_MODULATION_8FSK,
            "Unexpected slow-profile modulation"
        )) {
        return 1;
    }

    HFTextModemConfig config{};
    char error[128] = {};
    if (!require(
            hftext_c_modem_config_for_profile(
                &profiles,
                HFTEXT_SPEED_PROFILE_FAST,
                profiles.rx_sample_rate,
                &config,
                error,
                sizeof(error)
            ) == HFTEXT_STATUS_OK,
            "Could not derive fast modem config"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(error[0] == '\0', "Unexpected validation error text")) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(
            config.modulation_mode == HFTEXT_MODULATION_8FSK,
            "Unexpected fast-profile modulation"
        )) {
        return 1;
    }

    return 0;
}
