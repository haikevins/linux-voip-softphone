#pragma once

#include <optional>
#include <string>
#include <vector>

namespace voip::audio {

enum class AudioDirection
{
    Capture,
    Playback,
};

struct AudioDeviceDescriptor
{
    int id{-1};
    std::string driver;
    std::string name;
    unsigned int input_channels{0};
    unsigned int output_channels{0};

    [[nodiscard]] bool supports(AudioDirection direction) const noexcept;
};

struct AudioDeviceSelector
{
    std::string name;
    std::optional<std::string> driver;
};

[[nodiscard]] int resolve_audio_device_id(const std::vector<AudioDeviceDescriptor> &devices,
                                          const AudioDeviceSelector &selector,
                                          AudioDirection direction);

} // namespace voip::audio
