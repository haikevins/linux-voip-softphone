#pragma once

#include "application/SipEngine.hpp"
#include "threading/WorkerExecutor.hpp"

#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace voip::application {

class ThreadedSipEngine final : public SipEngine, private SipEngineObserver
{
  public:
    using Factory = std::function<std::unique_ptr<SipEngine>()>;

    explicit ThreadedSipEngine(Factory factory);
    ~ThreadedSipEngine() override;

    ThreadedSipEngine(const ThreadedSipEngine &) = delete;
    ThreadedSipEngine &operator=(const ThreadedSipEngine &) = delete;
    ThreadedSipEngine(ThreadedSipEngine &&) = delete;
    ThreadedSipEngine &operator=(ThreadedSipEngine &&) = delete;

    void set_observer(SipEngineObserver *observer) noexcept override;

    void register_account() override;
    void make_call(const std::string &destination) override;
    void answer_call() override;
    void hangup_call() override;

    [[nodiscard]] std::thread::id worker_thread_id() const noexcept;

  private:
    using Command = std::function<void(SipEngine &)>;

    void dispatch_registration_command(Command command);
    void dispatch_call_command(std::string operation, Command command);
    void destroy_engine_on_worker() noexcept;

    void on_registration_succeeded() override;
    void on_registration_failed(std::string reason) override;
    void on_incoming_call(std::string peer) override;
    void on_remote_ringing() override;
    void on_call_connected() override;
    void on_call_ended() override;
    void on_call_failed(std::string reason) override;

    template <typename Callback> void notify_observer(Callback callback) noexcept
    {
        try {
            const std::lock_guard lock{observer_mutex_};
            if (observer_ != nullptr) {
                callback(*observer_);
            }
        } catch (...) {
            std::fputs("SIP observer failed while handling threaded callback\n", stderr);
        }
    }

    threading::WorkerExecutor worker_;
    std::unique_ptr<SipEngine> engine_;
    std::mutex observer_mutex_;
    SipEngineObserver *observer_{nullptr};
};

} // namespace voip::application
