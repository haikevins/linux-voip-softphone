#include "audio/AudioDeviceSelection.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

namespace {

using voip::audio::AudioDeviceDescriptor;
using voip::audio::AudioDeviceSelector;
using voip::audio::AudioDirection;
using voip::audio::resolve_audio_device_id;

std::vector<AudioDeviceDescriptor> make_devices()
{
    return {
        {5, "ALSA", "pipewire", 1U, 1U},       {6, "ALSA", "pulse", 1U, 1U},
        {10, "ALSA", "default", 1U, 1U},       {35, "ALSA", "capture-only", 1U, 0U},
        {17, "ALSA", "playback-only", 0U, 1U},
    };
}

TEST(AudioDeviceSelectionTest, ResolvesCaptureDeviceByStableName)
{
    const auto devices = make_devices();

    EXPECT_EQ(resolve_audio_device_id(devices, AudioDeviceSelector{"pipewire", std::nullopt},
                                      AudioDirection::Capture),
              5);
}

TEST(AudioDeviceSelectionTest, ResolvesPlaybackDeviceByStableName)
{
    const auto devices = make_devices();

    EXPECT_EQ(resolve_audio_device_id(devices, AudioDeviceSelector{"default", std::nullopt},
                                      AudioDirection::Playback),
              10);
}

TEST(AudioDeviceSelectionTest, UsesDriverToDisambiguateDuplicateNames)
{
    auto devices = make_devices();
    devices.push_back({42, "Virtual", "default", 1U, 1U});

    EXPECT_EQ(resolve_audio_device_id(devices,
                                      AudioDeviceSelector{"default", std::string{"Virtual"}},
                                      AudioDirection::Playback),
              42);
}

TEST(AudioDeviceSelectionTest, RejectsAmbiguousNameWithoutDriver)
{
    auto devices = make_devices();
    devices.push_back({42, "Virtual", "default", 1U, 1U});

    EXPECT_THROW(
        static_cast<void>(resolve_audio_device_id(
            devices, AudioDeviceSelector{"default", std::nullopt}, AudioDirection::Playback)),
        std::invalid_argument);
}

TEST(AudioDeviceSelectionTest, RejectsDeviceWithoutRequestedCapability)
{
    const auto devices = make_devices();

    EXPECT_THROW(
        static_cast<void>(resolve_audio_device_id(
            devices, AudioDeviceSelector{"capture-only", std::nullopt}, AudioDirection::Playback)),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(resolve_audio_device_id(
            devices, AudioDeviceSelector{"playback-only", std::nullopt}, AudioDirection::Capture)),
        std::invalid_argument);
}

TEST(AudioDeviceSelectionTest, RejectsMissingDevice)
{
    const auto devices = make_devices();

    EXPECT_THROW(
        static_cast<void>(resolve_audio_device_id(
            devices, AudioDeviceSelector{"missing", std::nullopt}, AudioDirection::Capture)),
        std::invalid_argument);
}

TEST(AudioDeviceSelectionTest, RejectsEmptySelectorName)
{
    const auto devices = make_devices();

    EXPECT_THROW(static_cast<void>(resolve_audio_device_id(
                     devices, AudioDeviceSelector{"", std::nullopt}, AudioDirection::Capture)),
                 std::invalid_argument);
}

TEST(AudioDeviceSelectionTest, ResolutionDoesNotDependOnEnumerationOrder)
{
    const std::vector<AudioDeviceDescriptor> devices = {
        {99, "ALSA", "pulse", 1U, 1U},
        {7, "ALSA", "pipewire", 1U, 1U},
        {3, "ALSA", "default", 1U, 1U},
    };

    EXPECT_EQ(resolve_audio_device_id(devices, AudioDeviceSelector{"pipewire", std::nullopt},
                                      AudioDirection::Capture),
              7);
}

} // namespace
