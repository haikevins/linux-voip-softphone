#pragma once

#include "application/CallManager.hpp"

#include <QObject>
#include <QString>

namespace voip::gui {

// QML sees only state; it never owns or accesses a SIP/PJSUA2 object.
// Default GUI is inert; combined Qt+PJSIP mode may inject a registration-only backend.
class GuiViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString registrationStatus READ registrationStatus NOTIFY statusChanged)
    Q_PROPERTY(bool callActionsEnabled READ callActionsEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(bool backendAvailable READ backendAvailable CONSTANT)
    Q_PROPERTY(bool registrationActionsEnabled READ registrationActionsEnabled NOTIFY statusChanged)

  public:
    explicit GuiViewModel(QObject *parent = nullptr);
    explicit GuiViewModel(application::CallBackend &backend, QObject *parent = nullptr);

    [[nodiscard]] QString registrationStatus() const;
    [[nodiscard]] bool callActionsEnabled() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool backendAvailable() const;
    [[nodiscard]] bool registrationActionsEnabled() const;
    Q_INVOKABLE bool registerAccount();

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
