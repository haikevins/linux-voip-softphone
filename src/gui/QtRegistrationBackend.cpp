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
        worker_.join();
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
        answer_requested_ = false;
        hangup_requested_ = false;
    }
    incoming_call_active_ = false;
    call_active_ = true;
    stop_changed_.notify_all();
    return true;
}

bool QtRegistrationBackend::begin_answer()
{
    if (QThread::currentThread() != thread() || !supports_calls() || !call_active_ ||
        !incoming_call_active_) {
        return false;
    }
    {
        const std::lock_guard lock{stop_mutex_};
        if (answer_requested_ || hangup_requested_) {
            return false;
        }
        answer_requested_ = true;
    }
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
        answer_requested_ = false;
        hangup_requested_ = true;
    }
    stop_changed_.notify_all();
    return true;
}

bool QtRegistrationBackend::take_answer()
{
    const std::lock_guard lock{stop_mutex_};
    return std::exchange(answer_requested_, false);
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

void QtRegistrationBackend::post_incoming_call(std::string peer)
{
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, peer = QString::fromStdString(peer)] {
            call_active_ = true;
            incoming_call_active_ = true;
            emit incomingCall(peer);
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
                incoming_call_active_ = false;
                emit callConnected();
                break;
            case WorkerEvent::Ended:
                call_active_ = false;
                incoming_call_active_ = false;
                emit callEnded();
                break;
            case WorkerEvent::Failed:
                call_active_ = false;
                incoming_call_active_ = false;
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
            throw std::runtime_error("SIP call did not disconnect safely");
        }
        if (!session.release_disconnected_call()) {
            throw std::runtime_error("SIP call cleanup did not complete safely");
        }
    } catch (const std::exception &) {
        failed = true;
        if (!stop.stop_requested()) {
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

void QtRegistrationBackend::handle_incoming_call(RegistrationSession &session,
                                                 const IncomingCallNotice &notice,
                                                 const std::stop_token stop)
{
    post_incoming_call(notice.peer);

    bool answered = false;
    bool connected = false;
    bool failed = false;
    bool locally_cancelled = false;
    auto answer_deadline = std::chrono::steady_clock::time_point::max();

    try {
        if (!notice.disconnected) {
            while (!stop.stop_requested()) {
                if (take_hangup()) {
                    locally_cancelled = true;
                    session.hangup_call();
                }
                if (!locally_cancelled && !answered && take_answer()) {
                    session.answer_call();
                    answered = true;
                    answer_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
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
                    } else if (progress == CallProgress::DisconnectedAfterConnect) {
                        connected = true;
                        break;
                    } else if (progress == CallProgress::Disconnected) {
                        break;
                    } else if (progress == CallProgress::Failed) {
                        failed = true;
                        session.hangup_call();
                        break;
                    }
                }

                if (answered && !connected && std::chrono::steady_clock::now() >= answer_deadline) {
                    failed = true;
                    session.hangup_call();
                    break;
                }
            }
        }

        if (stop.stop_requested()) {
            session.hangup_call();
        }
        if (!session.wait_for_disconnection(std::chrono::seconds{2})) {
            throw std::runtime_error("incoming SIP call did not disconnect safely");
        }
        if (!session.release_disconnected_call()) {
            throw std::runtime_error("incoming SIP call cleanup did not complete safely");
        }
    } catch (const std::exception &) {
        failed = true;
        if (!stop.stop_requested()) {
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
            if (calls_enabled_ && !session->supports_calls()) {
                registered = false;
                throw std::runtime_error("factory returned a registration-only session");
            }
            post_registration(true);
            while (!stop.stop_requested()) {
                std::optional<std::string> destination;
                {
                    std::unique_lock lock{stop_mutex_};
                    if (session->supports_incoming_calls()) {
                        static_cast<void>(stop_changed_.wait_for(
                            lock, std::chrono::milliseconds{100}, [this, stop] {
                                return stop.stop_requested() || pending_destination_.has_value();
                            }));
                    } else {
                        stop_changed_.wait(lock, [this, stop] {
                            return stop.stop_requested() || pending_destination_.has_value();
                        });
                    }
                    if (stop.stop_requested()) {
                        break;
                    }
                    destination = std::move(pending_destination_);
                    pending_destination_.reset();
                }

                if (destination) {
                    handle_outgoing_call(*session, *destination, stop);
                    continue;
                }

                if (session->supports_incoming_calls()) {
                    const auto notice = session->poll_incoming_call(std::chrono::milliseconds{50});
                    if (notice.received) {
                        handle_incoming_call(*session, notice, stop);
                    }
                }
            }
        } else {
            post_registration(false);
        }
    } catch (const std::exception &) {
        if (!registered) {
            post_registration(false);
        } else {
            static_cast<void>(QMetaObject::invokeMethod(
                this, [this] { fatal_call_error_ = true; }, Qt::QueuedConnection));
            post_call_event(WorkerEvent::Failed);
        }
    } catch (...) {
        if (!registered) {
            post_registration(false);
        } else {
            static_cast<void>(QMetaObject::invokeMethod(
                this, [this] { fatal_call_error_ = true; }, Qt::QueuedConnection));
            post_call_event(WorkerEvent::Failed);
        }
    }
    if (session) {
        session->stop();
    }
}

} // namespace voip::gui
