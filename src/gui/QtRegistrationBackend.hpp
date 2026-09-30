#pragma once

#include "application/CallManager.hpp"

#include <QObject>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <thread>

namespace voip::gui {

// Qt-free session contract. Implementations own their SIP resources on the
// worker thread and retain registration until stop() has completed there.
class RegistrationSession
{
  public:
    virtual ~RegistrationSession() = default;
    virtual void start() = 0;
    [[nodiscard]] virtual bool wait_for_registration(std::chrono::milliseconds timeout) = 0;
    virtual void stop() noexcept = 0;
};

// Qt-thread-owned bridge; never exposes credentials or pj:: objects to QML.
// A one-shot worker performs blocking SIP startup and registration. The
// result is always dispatched back onto this QObject's owning Qt thread.
class QtRegistrationBackend final : public QObject, public application::CallBackend
{
    Q_OBJECT

  public:
    using SessionFactory = std::function<std::unique_ptr<RegistrationSession>()>;

    explicit QtRegistrationBackend(SessionFactory factory, QObject *parent = nullptr);
    ~QtRegistrationBackend() override;

    QtRegistrationBackend(const QtRegistrationBackend &) = delete;
    QtRegistrationBackend &operator=(const QtRegistrationBackend &) = delete;

    [[nodiscard]] bool begin_registration() override;
    [[nodiscard]] bool supports_calls() const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool begin_outgoing_call(std::string_view destination) override;
    [[nodiscard]] bool begin_hangup() override;

  signals:
    // Emitted on the Qt owning thread. No raw SIP error strings cross here.
    void registrationFinished(bool registered);

  private:
    void run_session(std::stop_token stop);
    void post_result(bool registered);

    SessionFactory factory_;
    std::mutex stop_mutex_;
    std::condition_variable stop_changed_;
    std::jthread worker_;
    bool request_active_{false};   // Accessed only on the Qt owning thread.
    bool session_retained_{false}; // A successful session must not be joined on retry.
};

} // namespace voip::gui
