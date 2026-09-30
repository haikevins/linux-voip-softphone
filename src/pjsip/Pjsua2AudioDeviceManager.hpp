#pragma once

#include "audio/AudioDeviceSelection.hpp"

#include <pjsua2.hpp>

#include <vector>

namespace voip::pjsip {

struct Pjsua2AudioDeviceIds
{
    int capture_id{-1};
    int playback_id{-1};
};

[[nodiscard]] std::vector<audio::AudioDeviceDescriptor>
to_audio_device_descriptors(const pj::AudioDevInfoVector2 &devices);

[[nodiscard]] Pjsua2AudioDeviceIds
resolve_pjsua2_audio_device_ids(const pj::AudioDevInfoVector2 &devices,
                                const audio::AudioDeviceSelector &capture_selector,
                                const audio::AudioDeviceSelector &playback_selector);

[[nodiscard]] Pjsua2AudioDeviceIds
configure_pjsua2_audio_devices(pj::AudDevManager &manager,
                               const audio::AudioDeviceSelector &capture_selector,
                               const audio::AudioDeviceSelector &playback_selector);

} // namespace voip::pjsip
