#include "gui/GuiViewModel.hpp"

#include "core/CallState.hpp"

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

} // namespace voip::gui
