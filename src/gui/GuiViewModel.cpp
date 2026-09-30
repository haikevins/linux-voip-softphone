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
           (current.state == CallState::Idle || current.state == CallState::Failed);
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

application::CallManager &GuiViewModel::manager() noexcept
{
    return manager_;
}

} // namespace voip::gui
