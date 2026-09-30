#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <cstddef>

namespace voip::pjsip {

std::vector<unsigned> active_audio_media_indices(const pj::CallInfo &call_info)
{
    std::vector<unsigned> indices;
    indices.reserve(call_info.media.size());

    for (std::size_t index = 0; index < call_info.media.size(); ++index) {
        const auto &media = call_info.media[index];
        if (media.type != PJMEDIA_TYPE_AUDIO || media.status != PJSUA_CALL_MEDIA_ACTIVE) {
            continue;
        }

        indices.push_back(static_cast<unsigned>(index));
    }

    return indices;
}

void connect_active_call_audio(pj::Call &call, pj::AudDevManager &audio_devices)
{
    const auto call_info = call.getInfo();

    for (const auto media_index : active_audio_media_indices(call_info)) {
        auto call_audio = call.getAudioMedia(static_cast<int>(media_index));
        call_audio.startTransmit(audio_devices.getPlaybackDevMedia());
        audio_devices.getCaptureDevMedia().startTransmit(call_audio);
    }
}

} // namespace voip::pjsip
