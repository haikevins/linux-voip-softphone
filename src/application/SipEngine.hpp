#pragma once

#include <string>

namespace voip::application {

class SipEngineObserver
{
  public:
    virtual ~SipEngineObserver() = default;

    virtual void on_registration_succeeded() = 0;
    virtual void on_registration_failed(std::string reason) = 0;
    virtual void on_incoming_call(std::string peer) = 0;
    virtual void on_remote_ringing() = 0;
    virtual void on_call_connected() = 0;
    virtual void on_call_ended() = 0;
    virtual void on_call_failed(std::string reason) = 0;
};

class SipEngine
{
  public:
    virtual ~SipEngine() = default;

    virtual void set_observer(SipEngineObserver *observer) noexcept = 0;

    virtual void register_account() = 0;
    virtual void make_call(const std::string &destination) = 0;
    virtual void answer_call() = 0;
    virtual void hangup_call() = 0;
};

} // namespace voip::application
