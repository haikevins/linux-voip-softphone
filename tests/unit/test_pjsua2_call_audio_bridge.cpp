#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <gtest/gtest.h>

namespace voip::pjsip {
namespace {

pj::CallMediaInfo make_media(const pjmedia_type type, const pjsua_call_media_status status)
{
    pj::CallMediaInfo media;
    media.type = type;
    media.status = status;
    return media;
}

TEST(Pjsua2CallAudioBridgeTest, FindsActiveAudioMedia)
{
    pj::CallInfo info;
    info.media = {make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_ACTIVE)};

    EXPECT_EQ(active_audio_media_indices(info), (std::vector<unsigned>{0U}));
}

TEST(Pjsua2CallAudioBridgeTest, IgnoresInactiveAudioMedia)
{
    pj::CallInfo info;
    info.media = {make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_NONE),
                  make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_REMOTE_HOLD)};

    EXPECT_TRUE(active_audio_media_indices(info).empty());
}

TEST(Pjsua2CallAudioBridgeTest, IgnoresActiveNonAudioMedia)
{
    pj::CallInfo info;
    info.media = {make_media(PJMEDIA_TYPE_VIDEO, PJSUA_CALL_MEDIA_ACTIVE)};

    EXPECT_TRUE(active_audio_media_indices(info).empty());
}

TEST(Pjsua2CallAudioBridgeTest, PreservesCallMediaIndexes)
{
    pj::CallInfo info;
    info.media = {make_media(PJMEDIA_TYPE_VIDEO, PJSUA_CALL_MEDIA_ACTIVE),
                  make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_ACTIVE),
                  make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_LOCAL_HOLD),
                  make_media(PJMEDIA_TYPE_AUDIO, PJSUA_CALL_MEDIA_ACTIVE)};

    EXPECT_EQ(active_audio_media_indices(info), (std::vector<unsigned>{1U, 3U}));
}

} // namespace
} // namespace voip::pjsip
