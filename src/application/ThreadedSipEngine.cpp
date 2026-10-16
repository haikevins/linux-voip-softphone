#include "application/ThreadedSipEngine.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace voip::application {
namespace {

[[nodiscard]] std::string operation_failure(const std::string_view operation,
                                            const std::exception &error)
{
    return std::string{operation} + ": " + error.what();
}

[[nodiscard]] std::string unknown_operation_failure(const std::string_view operation)
{
    return std::string{operation} + ": unknown failure";
}

} // namespace

ThreadedSipEngine::ThreadedSipEngine(Factory factory)
{
    if (!factory) {
        throw std::invalid_argument("threaded SIP engine factory must not be empty");
    }

    auto initialization = worker_.dispatch([this, factory = std::move(factory)]() mutable {
        engine_ = factory();
        if (engine_ == nullptr) {
            throw std::runtime_error("threaded SIP engine factory returned no engine");
        }
        engine_->set_observer(this);
    });
    initialization.get();
}

ThreadedSipEngine::~ThreadedSipEngine()
{
    {
        const std::lock_guard lock{observer_mutex_};
        observer_ = nullptr;
    }

    destroy_engine_on_worker();
    worker_.shutdown();
}

void ThreadedSipEngine::set_observer(SipEngineObserver *observer) noexcept
{
    const std::lock_guard lock{observer_mutex_};
    observer_ = observer;
}

void ThreadedSipEngine::register_account()
{
    dispatch_registration_command([](SipEngine &engine) { engine.register_account(); });
}

void ThreadedSipEngine::make_call(const std::string &destination)
{
    const auto command = [destination](SipEngine &engine) { engine.make_call(destination); };
    dispatch_call_command("make call", command);
}

void ThreadedSipEngine::answer_call()
{
    dispatch_call_command("answer call", [](SipEngine &engine) { engine.answer_call(); });
}

void ThreadedSipEngine::hangup_call()
{
    dispatch_call_command("hang up call", [](SipEngine &engine) { engine.hangup_call(); });
}

std::thread::id ThreadedSipEngine::worker_thread_id() const noexcept
{
    return worker_.worker_thread_id();
}

void ThreadedSipEngine::dispatch_registration_command(Command command)
{
    static_cast<void>(worker_.dispatch([this, command = std::move(command)]() mutable {
        try {
            command(*engine_);
        } catch (const std::exception &error) {
            on_registration_failed(operation_failure("register account", error));
        } catch (...) {
            on_registration_failed(unknown_operation_failure("register account"));
        }
    }));
}

void ThreadedSipEngine::dispatch_call_command(std::string operation, Command command)
{
    static_cast<void>(worker_.dispatch(
        [this, operation = std::move(operation), command = std::move(command)]() mutable {
            try {
                command(*engine_);
            } catch (const std::exception &error) {
                on_call_failed(operation_failure(operation, error));
            } catch (...) {
                on_call_failed(unknown_operation_failure(operation));
            }
        }));
}

void ThreadedSipEngine::destroy_engine_on_worker() noexcept
{
    if (!worker_.accepting()) {
        return;
    }

    try {
        auto destruction = worker_.dispatch([this]() {
            if (engine_ != nullptr) {
                engine_->set_observer(nullptr);
                engine_.reset();
            }
        });
        destruction.get();
    } catch (...) {
        std::fputs("Threaded SIP engine cleanup failed\n", stderr);
    }
}

void ThreadedSipEngine::on_registration_succeeded()
{
    notify_observer([](SipEngineObserver &observer) { observer.on_registration_succeeded(); });
}

void ThreadedSipEngine::on_registration_failed(std::string reason)
{
    notify_observer([reason = std::move(reason)](SipEngineObserver &observer) mutable {
        observer.on_registration_failed(std::move(reason));
    });
}

void ThreadedSipEngine::on_incoming_call(std::string peer)
{
    notify_observer([peer = std::move(peer)](SipEngineObserver &observer) mutable {
        observer.on_incoming_call(std::move(peer));
    });
}

void ThreadedSipEngine::on_remote_ringing()
{
    notify_observer([](SipEngineObserver &observer) { observer.on_remote_ringing(); });
}

void ThreadedSipEngine::on_call_connected()
{
    notify_observer([](SipEngineObserver &observer) { observer.on_call_connected(); });
}

void ThreadedSipEngine::on_call_ended()
{
    notify_observer([](SipEngineObserver &observer) { observer.on_call_ended(); });
}

void ThreadedSipEngine::on_call_failed(std::string reason)
{
    notify_observer([reason = std::move(reason)](SipEngineObserver &observer) mutable {
        observer.on_call_failed(std::move(reason));
    });
}

} // namespace voip::application
