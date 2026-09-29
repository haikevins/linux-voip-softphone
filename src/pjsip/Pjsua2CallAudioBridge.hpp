#pragma once

#include <pjsua2.hpp>

#include <vector>

namespace voip::pjsip {

[[nodiscard]] std::vector<unsigned> active_audio_media_indices(const pj::CallInfo &call_info);

void connect_active_call_audio(pj::Call &call, pj::AudDevManager &audio_devices);

} // namespace voip::pjsip
