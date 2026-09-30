#include "pjsip/Pjsua2AudioDeviceManager.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct AudioChannelCounts
{
    unsigned input{};
    unsigned output{};
};

pj::AudioDevInfo make_audio_device(const int id, std::string driver, std::string name,
                                   const AudioChannelCounts channels)
{
    pj::AudioDevInfo device;
    device.id = id;
    device.driver = std::move(driver);
    device.name = std::move(name);
    device.inputCount = channels.input;
    device.outputCount = channels.output;
    device.defaultSamplesPerSec = 8000U;
    device.caps = 0U;
    device.routes = 0U;
    return device;
}

} // namespace

TEST(Pjsua2AudioDeviceManagerTest, ConvertsPjsua2DeviceInfoToCoreDescriptors)
{
    const pj::AudioDevInfoVector2 devices{
        make_audio_device(5, "ALSA", "pipewire", AudioChannelCounts{1U, 1U}),
        make_audio_device(17, "ALSA", "hdmi:CARD=Generic,DEV=0", AudioChannelCounts{0U, 1U}),
    };

    const auto descriptors = voip::pjsip::to_audio_device_descriptors(devices);

    ASSERT_EQ(descriptors.size(), 2U);
    EXPECT_EQ(descriptors[0].id, 5);
    EXPECT_EQ(descriptors[0].driver, "ALSA");
    EXPECT_EQ(descriptors[0].name, "pipewire");
    EXPECT_EQ(descriptors[0].input_channels, 1U);
    EXPECT_EQ(descriptors[0].output_channels, 1U);
    EXPECT_EQ(descriptors[1].id, 17);
    EXPECT_EQ(descriptors[1].input_channels, 0U);
    EXPECT_EQ(descriptors[1].output_channels, 1U);
}

TEST(Pjsua2AudioDeviceManagerTest, ResolvesCaptureAndPlaybackIdsFromEnumeration)
{
    const pj::AudioDevInfoVector2 devices{
        make_audio_device(5, "ALSA", "pipewire", AudioChannelCounts{1U, 1U}),
        make_audio_device(6, "ALSA", "pulse", AudioChannelCounts{1U, 1U}),
        make_audio_device(17, "ALSA", "hdmi:CARD=Generic,DEV=0", AudioChannelCounts{0U, 1U}),
    };

    const auto ids = voip::pjsip::resolve_pjsua2_audio_device_ids(
        devices, voip::audio::AudioDeviceSelector{"pulse", std::string{"ALSA"}},
        voip::audio::AudioDeviceSelector{"pipewire", std::string{"ALSA"}});

    EXPECT_EQ(ids.capture_id, 6);
    EXPECT_EQ(ids.playback_id, 5);
}

TEST(Pjsua2AudioDeviceManagerTest, PreservesCapabilityValidationFromCorePolicy)
{
    const pj::AudioDevInfoVector2 devices{
        make_audio_device(17, "ALSA", "hdmi:CARD=Generic,DEV=0", AudioChannelCounts{0U, 1U}),
    };

    EXPECT_THROW(
        static_cast<void>(voip::pjsip::resolve_pjsua2_audio_device_ids(
            devices,
            voip::audio::AudioDeviceSelector{"hdmi:CARD=Generic,DEV=0", std::string{"ALSA"}},
            voip::audio::AudioDeviceSelector{"hdmi:CARD=Generic,DEV=0", std::string{"ALSA"}})),
        std::invalid_argument);
}
