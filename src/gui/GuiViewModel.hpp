#pragma once

#include "application/CallManager.hpp"

#include <QObject>
#include <QString>

namespace voip::gui {

// QML sees only state; it never owns or accesses a SIP/PJSUA2 object.
// The shipping GUI has no backend until a later commit wires SIP lifecycle.
class GuiViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString registrationStatus READ registrationStatus NOTIFY statusChanged)
    Q_PROPERTY(bool callActionsEnabled READ callActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)

  public:
    explicit GuiViewModel(QObject *parent = nullptr);
    explicit GuiViewModel(application::CallBackend &backend, QObject *parent = nullptr);

    [[nodiscard]] QString registrationStatus() const;
    [[nodiscard]] bool callActionsEnabled() const;
    [[nodiscard]] QString lastError() const;

    // C++ integration seam only. Future backend event handlers must post
    // lifecycle events onto this QObject's Qt thread before calling manager().
    [[nodiscard]] application::CallManager &manager() noexcept;

  signals:
    void statusChanged();

  private:
    void subscribe();

    application::CallManager manager_;
};

} // namespace voip::gui
