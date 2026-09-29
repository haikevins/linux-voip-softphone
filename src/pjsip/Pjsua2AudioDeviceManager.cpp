#include "pjsip/Pjsua2AudioDeviceManager.hpp"

namespace voip::pjsip {

std::vector<audio::AudioDeviceDescriptor>
to_audio_device_descriptors(const pj::AudioDevInfoVector2 &devices)
{
    std::vector<audio::AudioDeviceDescriptor> descriptors;
    descriptors.reserve(devices.size());

    for (const auto &device : devices) {
        descriptors.push_back(audio::AudioDeviceDescriptor{
            .id = static_cast<int>(device.id),
            .driver = device.driver,
            .name = device.name,
            .input_channels = device.inputCount,
            .output_channels = device.outputCount,
        });
    }

    return descriptors;
}

Pjsua2AudioDeviceIds
resolve_pjsua2_audio_device_ids(const pj::AudioDevInfoVector2 &devices,
                                const audio::AudioDeviceSelector &capture_selector,
                                const audio::AudioDeviceSelector &playback_selector)
{
    const auto descriptors = to_audio_device_descriptors(devices);

    return Pjsua2AudioDeviceIds{
        .capture_id = audio::resolve_audio_device_id(descriptors, capture_selector,
                                                     audio::AudioDirection::Capture),
        .playback_id = audio::resolve_audio_device_id(descriptors, playback_selector,
                                                      audio::AudioDirection::Playback),
    };
}

Pjsua2AudioDeviceIds
configure_pjsua2_audio_devices(pj::AudDevManager &manager,
                               const audio::AudioDeviceSelector &capture_selector,
                               const audio::AudioDeviceSelector &playback_selector)
{
    const auto device_ids =
        resolve_pjsua2_audio_device_ids(manager.enumDev2(), capture_selector, playback_selector);

    manager.setCaptureDev(device_ids.capture_id);
    manager.setPlaybackDev(device_ids.playback_id);

    return device_ids;
}

} // namespace voip::pjsip
