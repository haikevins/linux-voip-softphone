#pragma once

#include "audio/AudioDeviceSelection.hpp"

namespace voip::pjsip {

struct SipCallAudioConfig
{
    audio::AudioDeviceSelector capture_device{"default", std::nullopt};
    audio::AudioDeviceSelector playback_device{"default", std::nullopt};
};

} // namespace voip::pjsip
