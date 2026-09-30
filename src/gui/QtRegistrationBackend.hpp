#pragma once

#include "application/CallManager.hpp"

#include <QObject>
#include <QString>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

namespace voip::gui {

enum class CallProgress
{
    Pending,
    Ringing,
    ConnectedWithMedia,
    DisconnectedAfterConnect,
    Disconnected,
    Failed,
};

struct IncomingCallNotice
{
    bool received{false};
    bool disconnected{false};
    std::string peer;
};

struct RtpDiagnosticsSnapshot
{
    bool available{false};
    std::string codec;
    std::uint64_t packets_received{0};
    std::uint64_t packets_sent{0};
    std::uint64_t packets_lost{0};
    double packet_loss_percent{0.0};
    double jitter_ms{0.0};
    double round_trip_time_ms{0.0};
};

class RegistrationSession
{
  public:
    virtual ~RegistrationSession() = default;
    virtual void start() = 0;
    [[nodiscard]] virtual bool wait_for_registration(std::chrono::milliseconds timeout) = 0;
    virtual void stop() noexcept = 0;

    // Defaults preserve the inert and registration-only test seams. Production
    // call sessions opt in explicitly to outgoing and incoming capabilities.
    [[nodiscard]] virtual bool supports_calls() const noexcept
    {
        return false;
    }
    [[nodiscard]] virtual bool supports_incoming_calls() const noexcept
    {
        return false;
    }
    virtual void start_call(std::string_view /*extension*/) {}
    [[nodiscard]] virtual IncomingCallNotice
    poll_incoming_call(std::chrono::milliseconds /*timeout*/)
    {
        return {};
    }
    virtual void answer_call() {}
    virtual void decline_call() {}
    [[nodiscard]] virtual RtpDiagnosticsSnapshot read_rtp_diagnostics()
    {
        return {};
    }
    [[nodiscard]] virtual CallProgress poll_call(std::chrono::milliseconds /*timeout*/)
    {
        return CallProgress::Failed;
    }
    virtual void hangup_call() noexcept {}
    [[nodiscard]] virtual bool wait_for_disconnection(std::chrono::milliseconds /*timeout*/)
    {
        return false;
    }
    [[nodiscard]] virtual bool release_disconnected_call()
    {
        return false;
    }
};

// Main-thread CallBackend. All PJSUA2 operations are serialized on one worker.
// Worker results are posted with Qt::QueuedConnection and never invoke the
// CallManager synchronously from a SIP/PJSIP thread.
class QtRegistrationBackend final : public QObject, public application::CallBackend
{
    Q_OBJECT

  public:
    using SessionFactory = std::function<std::unique_ptr<RegistrationSession>()>;

    explicit QtRegistrationBackend(SessionFactory factory, bool enable_calls = false,
                                   QObject *parent = nullptr);
    ~QtRegistrationBackend() override;

    QtRegistrationBackend(const QtRegistrationBackend &) = delete;
    QtRegistrationBackend &operator=(const QtRegistrationBackend &) = delete;

    [[nodiscard]] bool begin_registration() override;
    [[nodiscard]] bool supports_calls() const noexcept override;
    [[nodiscard]] bool begin_outgoing_call(std::string_view destination) override;
    [[nodiscard]] bool begin_answer() override;
    [[nodiscard]] bool begin_decline() override;
    [[nodiscard]] bool begin_hangup() override;

  signals:
    void registrationFinished(bool registered);
    void incomingCall(QString peer); // Sanitized extension/label only, never a raw SIP URI.
    void callRinging();
    void callConnected(); // Only after PJSIP confirms BOTH call and audio media.
    void rtpDiagnosticsUpdated(QString codec, qulonglong packetsReceived, qulonglong packetsSent,
                               qulonglong packetsLost, double packetLossPercent, double jitterMs,
                               double roundTripTimeMs);
    void rtpDiagnosticsCleared();
    void callEnded();
    void callFailed(); // No untrusted SIP error strings escape the worker.

  private:
    enum class WorkerEvent
    {
        Ringing,
        Connected,
        Ended,
        Failed
    };

    void run_session(std::stop_token stop);
    void handle_outgoing_call(RegistrationSession &session, const std::string &destination,
                              std::stop_token stop);
    void handle_incoming_call(RegistrationSession &session, const IncomingCallNotice &notice,
                              std::stop_token stop);
    void post_registration(bool registered);
    void post_incoming_call(std::string peer);
    void post_call_event(WorkerEvent event);
    void post_rtp_diagnostics(RtpDiagnosticsSnapshot diagnostics);
    void
    publish_rtp_diagnostics_if_due(RegistrationSession &session,
                                   std::chrono::steady_clock::time_point &next_update) noexcept;
    [[nodiscard]] bool take_answer();
    [[nodiscard]] bool take_decline();
    [[nodiscard]] bool take_hangup();

    SessionFactory factory_;
    const bool calls_enabled_;
    std::mutex stop_mutex_;
    std::condition_variable stop_changed_;
    std::jthread worker_;
    std::optional<std::string> pending_destination_; // stop_mutex_ protected.
    bool answer_requested_{false};                   // stop_mutex_ protected.
    bool decline_requested_{false};                  // stop_mutex_ protected.
    bool hangup_requested_{false};                   // stop_mutex_ protected.
    bool request_active_{false};                     // Qt thread only.
    bool session_retained_{false};                   // Qt thread only.
    bool call_active_{false};                        // Qt thread only.
    bool incoming_call_active_{false};               // Qt thread only.
    bool fatal_call_error_{false};                   // Qt thread only.
};

} // namespace voip::gui
