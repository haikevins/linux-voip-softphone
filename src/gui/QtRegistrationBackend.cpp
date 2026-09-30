#include "gui/QtRegistrationBackend.hpp"

#include <QMetaObject>
#include <QThread>

#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace voip::gui {

QtRegistrationBackend::QtRegistrationBackend(SessionFactory factory, const bool enable_calls,
                                             QObject *parent)
    : QObject(parent), factory_(std::move(factory)), calls_enabled_(enable_calls)
{
    if (!factory_) {
        throw std::invalid_argument("registration session factory must not be empty");
    }
}

QtRegistrationBackend::~QtRegistrationBackend()
{
    worker_.request_stop();
    stop_changed_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool QtRegistrationBackend::supports_calls() const noexcept
{
    return calls_enabled_ && session_retained_ && !fatal_call_error_;
}

bool QtRegistrationBackend::begin_registration()
{
    if (QThread::currentThread() != thread() || request_active_ || session_retained_) {
        return false;
    }
    if (worker_.joinable()) {
        worker_.join(); // Only an earlier failed registration may be joined here.
    }

    request_active_ = true;
    try {
        worker_ = std::jthread([this](const std::stop_token stop) { run_session(stop); });
    } catch (const std::system_error &) {
        request_active_ = false;
        return false;
    }
    return true;
}

bool QtRegistrationBackend::begin_outgoing_call(const std::string_view destination)
{
    if (QThread::currentThread() != thread() || !supports_calls() || call_active_ ||
        destination.empty()) {
        return false;
    }
    {
        const std::lock_guard lock{stop_mutex_};
        if (pending_destination_) {
            return false;
        }
        pending_destination_ = std::string{destination};
        hangup_requested_ = false;
    }
    call_active_ = true;
    stop_changed_.notify_all();
    return true;
}

bool QtRegistrationBackend::begin_hangup()
{
    if (QThread::currentThread() != thread() || !call_active_) {
        return false;
    }
    {
        const std::lock_guard lock{stop_mutex_};
        hangup_requested_ = true;
    }
    stop_changed_.notify_all();
    return true;
}

bool QtRegistrationBackend::take_hangup()
{
    const std::lock_guard lock{stop_mutex_};
    return std::exchange(hangup_requested_, false);
}

void QtRegistrationBackend::post_registration(const bool registered)
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, registered] {
            request_active_ = false;
            session_retained_ = registered;
            emit registrationFinished(registered);
        },
        Qt::QueuedConnection));
}

void QtRegistrationBackend::post_call_event(const WorkerEvent event)
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, event] {
            switch (event) {
            case WorkerEvent::Ringing:
                emit callRinging();
                break;
            case WorkerEvent::Connected:
                emit callConnected();
                break;
            case WorkerEvent::Ended:
                call_active_ = false;
                emit callEnded();
                break;
            case WorkerEvent::Failed:
                call_active_ = false;
                emit callFailed();
                break;
            }
        },
        Qt::QueuedConnection));
}

void QtRegistrationBackend::handle_outgoing_call(RegistrationSession &session,
                                                 const std::string &destination,
                                                 const std::stop_token stop)
{
    // Cancellation of a request before it reaches PJSUA2 produces no INVITE.
    if (take_hangup()) {
        post_call_event(WorkerEvent::Ended);
        return;
    }

    bool connected = false;
    bool locally_cancelled = false;
    bool failed = false;
    bool ringing_reported = false;
    try {
        session.start_call(destination);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
        while (!stop.stop_requested()) {
            if (take_hangup()) {
                locally_cancelled = true;
                session.hangup_call();
            }
            if (connected) {
                if (session.wait_for_disconnection(std::chrono::milliseconds{150})) {
                    break;
                }
            } else {
                const auto progress = session.poll_call(std::chrono::milliseconds{150});
                if (progress == CallProgress::ConnectedWithMedia) {
                    connected = true;
                    if (!locally_cancelled) {
                        post_call_event(WorkerEvent::Connected);
                    }
                } else if (progress == CallProgress::Ringing && !ringing_reported &&
                           !locally_cancelled) {
                    ringing_reported = true;
                    post_call_event(WorkerEvent::Ringing);
                } else if (progress == CallProgress::DisconnectedAfterConnect) {
                    // The call reached confirmed media but the remote side
                    // disconnected before this worker published Connected.
                    // That is a successful remote hangup, not a call failure.
                    connected = true;
                    break;
                } else if (progress == CallProgress::Disconnected) {
                    failed = !locally_cancelled && !connected;
                    break;
                } else if (progress == CallProgress::Failed) {
                    failed = true;
                    session.hangup_call();
                    break;
                }
            }
            if (!connected && std::chrono::steady_clock::now() >= deadline) {
                failed = true;
                session.hangup_call();
                break;
            }
        }
        if (stop.stop_requested()) {
            session.hangup_call();
        }
        if (!session.wait_for_disconnection(std::chrono::seconds{2})) {
            // Never delete an active PJSUA2 call wrapper and then start a new call.
            throw std::runtime_error("SIP call did not disconnect safely");
        }
        if (!session.release_disconnected_call()) {
            throw std::runtime_error("SIP call cleanup did not complete safely");
        }
    } catch (const std::exception &) {
        // No SIP library exception string is ever emitted toward QML.
        failed = true;
        if (!stop.stop_requested()) {
            // Mark a fatal worker problem by ending the session below.
            static_cast<void>(QMetaObject::invokeMethod(
                this, [this] { fatal_call_error_ = true; }, Qt::QueuedConnection));
        }
    } catch (...) {
        failed = true;
        static_cast<void>(QMetaObject::invokeMethod(
            this, [this] { fatal_call_error_ = true; }, Qt::QueuedConnection));
    }
    if (!stop.stop_requested()) {
        post_call_event(failed ? WorkerEvent::Failed : WorkerEvent::Ended);
    }
}

void QtRegistrationBackend::run_session(const std::stop_token stop)
{
    std::unique_ptr<RegistrationSession> session;
    bool registered = false;
    try {
        session = factory_();
        if (session) {
            session->start();
            registered =
                session->wait_for_registration(std::chrono::seconds{8}) && !stop.stop_requested();
        }
        if (registered) {
            // The factory's capability is checked before publication; an
            // unsupported factory must never advertise live outgoing calls.
            if (calls_enabled_ && !session->supports_calls()) {
                registered = false;
                throw std::runtime_error("factory returned a registration-only session");
            }
            post_registration(true);
            while (!stop.stop_requested()) {
                std::optional<std::string> destination;
                {
                    std::unique_lock lock{stop_mutex_};
                    stop_changed_.wait(lock, [this, stop] {
                        return stop.stop_requested() || pending_destination_.has_value();
                    });
                    if (stop.stop_requested()) {
                        break;
                    }
                    destination = std::move(pending_destination_);
                    pending_destination_.reset();
                }
                if (destination) {
                    handle_outgoing_call(*session, *destination, stop);
                }
            }
        } else {
            post_registration(false);
        }
    } catch (const std::exception &) {
        // Sanitized failure: never expose SIP error.info() or account details.
        if (!registered) {
            post_registration(false);
        } else {
            post_call_event(WorkerEvent::Failed);
        }
    } catch (...) {
        if (!registered) {
            post_registration(false);
        } else {
            post_call_event(WorkerEvent::Failed);
        }
    }
    if (session) {
        session->stop(); // The worker that constructed PJSIP also destroys it.
    }
}

} // namespace voip::gui
