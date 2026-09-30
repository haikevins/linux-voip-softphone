#include "gui/GuiViewModel.hpp"

#include "core/CallState.hpp"

#include <utility>

namespace voip::gui {

GuiViewModel::GuiViewModel(QObject *parent) : QObject(parent)
{
    subscribe();
}

GuiViewModel::GuiViewModel(application::CallBackend &backend, QObject *parent)
    : QObject(parent), manager_(&backend)
{
    subscribe();
}

void GuiViewModel::subscribe()
{
    manager_.set_observer([this](const application::CallSnapshot &) { emit statusChanged(); });
}

QString GuiViewModel::registrationStatus() const
{
    const auto current = manager_.snapshot();
    if (current.registration_confirmed) {
        return QStringLiteral("Registered");
    }
    switch (current.state) {
    case CallState::Idle:
        return current.backend_available ? QStringLiteral("Not registered")
                                         : QStringLiteral("Not connected - GUI shell only");
    case CallState::Registering:
        return QStringLiteral("Registering...");
    case CallState::Ready:
    case CallState::Calling:
    case CallState::Ringing:
    case CallState::Connected:
    case CallState::Terminating:
        return QStringLiteral("Registered");
    case CallState::Failed:
        return QStringLiteral("Registration or call failed");
    }
    return QStringLiteral("Unknown");
}

bool GuiViewModel::callActionsEnabled() const
{
    return manager_.snapshot().call_actions_enabled;
}

QString GuiViewModel::lastError() const
{
    return QString::fromStdString(manager_.snapshot().error);
}

bool GuiViewModel::backendAvailable() const
{
    return manager_.snapshot().backend_available;
}

bool GuiViewModel::registrationActionsEnabled() const
{
    const auto current = manager_.snapshot();
    return current.backend_available &&
           (current.state == CallState::Idle ||
            (current.state == CallState::Failed && !current.registration_confirmed));
}

bool GuiViewModel::registerAccount()
{
    if (!registrationActionsEnabled()) {
        return false;
    }
    if (manager_.snapshot().state == CallState::Failed && !manager_.reset_after_failure()) {
        return false;
    }
    return manager_.request_registration();
}

QString GuiViewModel::callStatus() const
{
    const auto current = manager_.snapshot();
    switch (current.state) {
    case CallState::Idle:
        return QStringLiteral("No active call");
    case CallState::Registering:
        return QStringLiteral("Waiting for registration");
    case CallState::Ready:
        return QStringLiteral("Ready to call");
    case CallState::Calling:
        return QStringLiteral("Calling...");
    case CallState::Ringing:
        if (current.incoming_call) {
            return current.answer_actions_enabled ? QStringLiteral("Incoming call")
                                                  : QStringLiteral("Answering...");
        }
        return QStringLiteral("Ringing...");
    case CallState::Connected:
        return QStringLiteral("Connected (audio active)");
    case CallState::Terminating:
        return QStringLiteral("Ending call...");
    case CallState::Failed:
        return QStringLiteral("Request failed");
    }
    return QStringLiteral("Unknown");
}

QString GuiViewModel::activePeer() const
{
    return QString::fromStdString(manager_.snapshot().peer);
}

bool GuiViewModel::incomingCall() const
{
    return manager_.snapshot().incoming_call;
}

bool GuiViewModel::answerActionsEnabled() const
{
    return manager_.snapshot().answer_actions_enabled;
}

bool GuiViewModel::declineActionsEnabled() const
{
    return manager_.snapshot().decline_actions_enabled;
}

bool GuiViewModel::callConnected() const
{
    return manager_.snapshot().state == CallState::Connected;
}

bool GuiViewModel::rtpDiagnosticsAvailable() const noexcept
{
    return rtp_diagnostics_available_;
}

QString GuiViewModel::rtpCodec() const
{
    return rtp_codec_;
}

qulonglong GuiViewModel::rtpPacketsReceived() const noexcept
{
    return rtp_packets_received_;
}

qulonglong GuiViewModel::rtpPacketsSent() const noexcept
{
    return rtp_packets_sent_;
}

qulonglong GuiViewModel::rtpPacketsLost() const noexcept
{
    return rtp_packets_lost_;
}

double GuiViewModel::rtpPacketLossPercent() const noexcept
{
    return rtp_packet_loss_percent_;
}

double GuiViewModel::rtpJitterMs() const noexcept
{
    return rtp_jitter_ms_;
}

double GuiViewModel::rtpRoundTripTimeMs() const noexcept
{
    return rtp_round_trip_time_ms_;
}

bool GuiViewModel::hangupActionsEnabled() const
{
    const auto state = manager_.snapshot().state;
    return state == CallState::Calling || state == CallState::Ringing ||
           state == CallState::Connected;
}

bool GuiViewModel::recoveryActionsEnabled() const
{
    return manager_.snapshot().recovery_actions_enabled;
}

bool GuiViewModel::startCall(const QString &destination)
{
    return manager_.request_outgoing_call(destination.toStdString());
}

bool GuiViewModel::answerCall()
{
    return manager_.request_answer();
}

bool GuiViewModel::declineCall()
{
    return manager_.request_decline();
}

bool GuiViewModel::hangupCall()
{
    return manager_.request_hangup();
}

bool GuiViewModel::recoverCall()
{
    if (!recoveryActionsEnabled()) {
        return false;
    }
    return manager_.reset_after_failure();
}

application::CallManager &GuiViewModel::manager() noexcept
{
    return manager_;
}

void GuiViewModel::updateRtpDiagnostics(QString codec, const qulonglong packets_received,
                                        const qulonglong packets_sent,
                                        const qulonglong packets_lost,
                                        const double packet_loss_percent, const double jitter_ms,
                                        const double round_trip_time_ms)
{
    rtp_diagnostics_available_ = true;
    rtp_codec_ = std::move(codec);
    rtp_packets_received_ = packets_received;
    rtp_packets_sent_ = packets_sent;
    rtp_packets_lost_ = packets_lost;
    rtp_packet_loss_percent_ = packet_loss_percent;
    rtp_jitter_ms_ = jitter_ms;
    rtp_round_trip_time_ms_ = round_trip_time_ms;
    emit rtpDiagnosticsChanged();
}

void GuiViewModel::clearRtpDiagnostics()
{
    rtp_diagnostics_available_ = false;
    rtp_codec_.clear();
    rtp_packets_received_ = 0;
    rtp_packets_sent_ = 0;
    rtp_packets_lost_ = 0;
    rtp_packet_loss_percent_ = 0.0;
    rtp_jitter_ms_ = 0.0;
    rtp_round_trip_time_ms_ = 0.0;
    emit rtpDiagnosticsChanged();
}

} // namespace voip::gui
