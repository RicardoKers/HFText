#include "hftext_config.h"
#include "hftext_result.h"

namespace hftext {

static_assert(ModemConfig{}.sampleRate == 48000, "unexpected default sample rate");
static_assert(ModemConfig{}.preambleBits == 64, "unexpected default preamble length");

}  // namespace hftext
