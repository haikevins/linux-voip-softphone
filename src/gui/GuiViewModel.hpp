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
    [[nodiscard]] bool hangupActionsEnabled() const;
    [[nodiscard]] bool recoveryActionsEnabled() const;
    Q_INVOKABLE bool startCall(const QString &destination);
    Q_INVOKABLE bool answerCall();
    Q_INVOKABLE bool declineCall();
    Q_INVOKABLE bool hangupCall();
    Q_INVOKABLE bool recoverCall();

    // C++ integration seam only. Backend handlers must post lifecycle events
    // onto this QObject's Qt thread before calling manager().
    [[nodiscard]] application::CallManager &manager() noexcept;

  signals:
    void statusChanged();

  private:
    void subscribe();

    application::CallManager manager_;
};

} // namespace voip::gui
