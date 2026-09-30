#include "gui/PjsipRegistrationSession.hpp"

#include <utility>

namespace voip::gui {

PjsipRegistrationSession::PjsipRegistrationSession(pjsip::SipEndpointConfig endpoint,
                                                   pjsip::SipAccountConfig account)
    : client_(endpoint, std::move(account))
{}

void PjsipRegistrationSession::start()
{
    client_.start();
}

bool PjsipRegistrationSession::wait_for_registration(const std::chrono::milliseconds timeout)
{
    const auto result = client_.wait_for_registration(timeout);
    return result.registered && !result.timed_out;
}

void PjsipRegistrationSession::stop() noexcept
{
    client_.stop();
}

} // namespace voip::gui
