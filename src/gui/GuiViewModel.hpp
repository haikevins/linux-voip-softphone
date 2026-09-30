#pragma once

#include <QObject>
#include <QString>

namespace voip::gui {

// Phase 7 Commit 1: read-only UI contract. It deliberately does not simulate
// successful registration or provide a fake call backend.
class GuiViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString registrationStatus READ registrationStatus CONSTANT)
    Q_PROPERTY(bool callActionsEnabled READ callActionsEnabled CONSTANT)

  public:
    explicit GuiViewModel(QObject *parent = nullptr);

    [[nodiscard]] QString registrationStatus() const;
    [[nodiscard]] bool callActionsEnabled() const noexcept;
};

} // namespace voip::gui
