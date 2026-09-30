#include "gui/QtRegistrationBackend.hpp"

#include <QMetaObject>
#include <QThread>

#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace voip::gui {

QtRegistrationBackend::QtRegistrationBackend(SessionFactory factory, QObject *parent)
    : QObject(parent), factory_(std::move(factory))
{
    if (!factory_) {
        throw std::invalid_argument("registration session factory must not be empty");
    }
}

QtRegistrationBackend::~QtRegistrationBackend()
{
    // An in-flight registration wait is bounded to eight seconds; SIP startup
    // and cleanup may add time. Retained sessions stop on this worker.
    worker_.request_stop();
    stop_changed_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool QtRegistrationBackend::begin_registration()
{
    if (QThread::currentThread() != thread() || request_active_ || session_retained_) {
        return false;
    }

    // A prior failed attempt has already posted its result and exited before
    // we allow another attempt. Joining it here must not block on network I/O.
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

bool QtRegistrationBackend::begin_outgoing_call(std::string_view /*destination*/)
{
    return false; // Commit 3 is registration-only.
}

bool QtRegistrationBackend::begin_hangup()
{
    return false;
}

void QtRegistrationBackend::post_result(const bool registered)
{
    // Queued delivery means CallManager is never re-entered by begin_registration().
    // This QObject outlives the worker; destruction joins before QObject cleanup.
    static_cast<void>(QMetaObject::invokeMethod(
        this,
        [this, registered] {
            request_active_ = false;
            session_retained_ = registered;
            emit registrationFinished(registered);
        },
        Qt::QueuedConnection));
}

void QtRegistrationBackend::run_session(const std::stop_token stop)
{
    try {
        auto session = factory_();
        if (!session) {
            post_result(false);
            return;
        }

        session->start();
        const bool registered =
            session->wait_for_registration(std::chrono::seconds{8}) && !stop.stop_requested();
        if (!registered) {
            session->stop();
            post_result(false);
            return;
        }

        // The endpoint stays alive after reporting success, and is destroyed
        // on its construction thread only after the GUI begins shutdown.
        post_result(true);
        std::unique_lock lock{stop_mutex_};
        stop_changed_.wait(lock, [stop] { return stop.stop_requested(); });
        lock.unlock();
        session->stop();
    } catch (const std::exception &) {
        // Do not forward exceptions: third-party text may contain credentials.
        post_result(false);
    } catch (...) {
        post_result(false);
    }
}

} // namespace voip::gui
