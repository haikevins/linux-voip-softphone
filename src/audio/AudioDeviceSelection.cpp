#include "audio/AudioDeviceSelection.hpp"

#include <stdexcept>

namespace voip::audio {

bool AudioDeviceDescriptor::supports(const AudioDirection direction) const noexcept
{
    switch (direction) {
    case AudioDirection::Capture:
        return input_channels > 0U;
    case AudioDirection::Playback:
        return output_channels > 0U;
    }

    return false;
}

int resolve_audio_device_id(const std::vector<AudioDeviceDescriptor> &devices,
                            const AudioDeviceSelector &selector, const AudioDirection direction)
{
    if (selector.name.empty()) {
        throw std::invalid_argument("audio device selector name must not be empty");
    }

    const AudioDeviceDescriptor *selected_device{nullptr};
    bool identity_match_found{false};

    for (const auto &device : devices) {
        if (device.name != selector.name) {
            continue;
        }
        if (selector.driver.has_value() && device.driver != selector.driver.value()) {
            continue;
        }

        identity_match_found = true;
        if (!device.supports(direction)) {
            continue;
        }

        if (selected_device != nullptr) {
            throw std::invalid_argument(
                "audio device selector is ambiguous; specify a unique driver and name");
        }

        selected_device = &device;
    }

    if (selected_device != nullptr) {
        return selected_device->id;
    }

    if (identity_match_found) {
        throw std::invalid_argument(
            "selected audio device does not support the requested direction");
    }

    throw std::invalid_argument("selected audio device was not found");
}

} // namespace voip::audio
