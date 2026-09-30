#include "gui/GuiViewModel.hpp"

namespace voip::gui {

GuiViewModel::GuiViewModel(QObject *parent) : QObject(parent) {}

QString GuiViewModel::registrationStatus() const
{
    return QStringLiteral("Not connected - GUI shell only");
}

bool GuiViewModel::callActionsEnabled() const noexcept
{
    return false;
}

} // namespace voip::gui
