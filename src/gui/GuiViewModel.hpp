#pragma once

#include "application/CallManager.hpp"

#include <QObject>
#include <QString>

namespace voip::gui {

// QML sees only sanitized state; it never owns or accesses a SIP/PJSUA2 object.
// Default GUI is inert; combined Qt+PJSIP mode opts in to live calls.
class GuiViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString registrationStatus READ registrationStatus NOTIFY statusChanged)
    Q_PROPERTY(bool callActionsEnabled READ callActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(bool backendAvailable READ backendAvailable CONSTANT)
    Q_PROPERTY(bool registrationActionsEnabled READ registrationActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString callStatus READ callStatus NOTIFY statusChanged)
    Q_PROPERTY(QString activePeer READ activePeer NOTIFY statusChanged)
    Q_PROPERTY(bool incomingCall READ incomingCall NOTIFY statusChanged)
    Q_PROPERTY(bool answerActionsEnabled READ answerActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool declineActionsEnabled READ declineActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool callConnected READ callConnected NOTIFY statusChanged)
    Q_PROPERTY(
        bool rtpDiagnosticsAvailable READ rtpDiagnosticsAvailable NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(QString rtpCodec READ rtpCodec NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(qulonglong rtpPacketsReceived READ rtpPacketsReceived NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(qulonglong rtpPacketsSent READ rtpPacketsSent NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(qulonglong rtpPacketsLost READ rtpPacketsLost NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(double rtpPacketLossPercent READ rtpPacketLossPercent NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(double rtpJitterMs READ rtpJitterMs NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(double rtpRoundTripTimeMs READ rtpRoundTripTimeMs NOTIFY rtpDiagnosticsChanged)
    Q_PROPERTY(bool hangupActionsEnabled READ hangupActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool recoveryActionsEnabled READ recoveryActionsEnabled NOTIFY statusChanged)

  public:
    explicit GuiViewModel(QObject *parent = nullptr);
    explicit GuiViewModel(application::CallBackend &backend, QObject *parent = nullptr);

    [[nodiscard]] QString registrationStatus() const;
    [[nodiscard]] bool callActionsEnabled() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool backendAvailable() const;
    [[nodiscard]] bool registrationActionsEnabled() const;
    Q_INVOKABLE bool registerAccount();
    [[nodiscard]] QString callStatus() const;
    [[nodiscard]] QString activePeer() const;
    [[nodiscard]] bool incomingCall() const;
    [[nodiscard]] bool answerActionsEnabled() const;
    [[nodiscard]] bool declineActionsEnabled() const;
    [[nodiscard]] bool callConnected() const;
    [[nodiscard]] bool rtpDiagnosticsAvailable() const noexcept;
    [[nodiscard]] QString rtpCodec() const;
    [[nodiscard]] qulonglong rtpPacketsReceived() const noexcept;
    [[nodiscard]] qulonglong rtpPacketsSent() const noexcept;
    [[nodiscard]] qulonglong rtpPacketsLost() const noexcept;
    [[nodiscard]] double rtpPacketLossPercent() const noexcept;
    [[nodiscard]] double rtpJitterMs() const noexcept;
    [[nodiscard]] double rtpRoundTripTimeMs() const noexcept;
    [[nodiscard]] bool hangupActionsEnabled() const;
    [[nodiscard]] bool recoveryActionsEnabled() const;
    Q_INVOKABLE bool startCall(const QString &destination);
    Q_INVOKABLE bool answerCall();
    Q_INVOKABLE bool declineCall();
    Q_INVOKABLE bool hangupCall();
    Q_INVOKABLE bool recoverCall();

    // C++ integration seam only. Backend handlers must post lifecycle events
    // onto this QObject's Qt thread before calling manager()/diagnostic updates.
    [[nodiscard]] application::CallManager &manager() noexcept;
    void updateRtpDiagnostics(QString codec, qulonglong packets_received, qulonglong packets_sent,
                              qulonglong packets_lost, double packet_loss_percent, double jitter_ms,
                              double round_trip_time_ms);
    void clearRtpDiagnostics();

  signals:
    void statusChanged();
    void rtpDiagnosticsChanged();

  private:
    void subscribe();

    application::CallManager manager_;
    bool rtp_diagnostics_available_{false};
    QString rtp_codec_;
    qulonglong rtp_packets_received_{0};
    qulonglong rtp_packets_sent_{0};
    qulonglong rtp_packets_lost_{0};
    double rtp_packet_loss_percent_{0.0};
    double rtp_jitter_ms_{0.0};
    double rtp_round_trip_time_ms_{0.0};
};

} // namespace voip::gui
