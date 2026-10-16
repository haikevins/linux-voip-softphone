#pragma once

#include "application/CallManager.hpp"
#include "application/SipEngine.hpp"
#include "core/StateChangeSink.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace voip::gui {

struct SipConnectionSettings
{
    std::string username;
    std::string password;
    std::string server;
    std::string realm{"*"};
    std::uint16_t server_port{5060U};
    std::uint16_t listen_port{0U};
    bool use_tcp{false};
};

using SipEngineFactory =
    std::function<std::unique_ptr<application::SipEngine>(const SipConnectionSettings &)>;

class CallViewModel final : public QObject, public StateChangeSink
{
    Q_OBJECT
    Q_PROPERTY(QString stateText READ state_text NOTIFY viewStateChanged)
    Q_PROPERTY(QString peer READ peer NOTIFY viewStateChanged)
    Q_PROPERTY(QString errorText READ error_text NOTIFY viewStateChanged)
    Q_PROPERTY(QString accountText READ account_text NOTIFY viewStateChanged)
    Q_PROPERTY(QString callDurationText READ call_duration_text NOTIFY callDurationChanged)
    Q_PROPERTY(bool incomingCall READ incoming_call NOTIFY viewStateChanged)
    Q_PROPERTY(bool canRegister READ can_register NOTIFY viewStateChanged)
    Q_PROPERTY(bool canCall READ can_call NOTIFY viewStateChanged)
    Q_PROPERTY(bool canAnswer READ can_answer NOTIFY viewStateChanged)
    Q_PROPERTY(bool canHangup READ can_hangup NOTIFY viewStateChanged)

  public:
    explicit CallViewModel(SipEngineFactory engine_factory, QObject *parent = nullptr);
    ~CallViewModel() override = default;

    CallViewModel(const CallViewModel &) = delete;
    CallViewModel &operator=(const CallViewModel &) = delete;
    CallViewModel(CallViewModel &&) = delete;
    CallViewModel &operator=(CallViewModel &&) = delete;

    [[nodiscard]] QString state_text() const;
    [[nodiscard]] QString peer() const;
    [[nodiscard]] QString error_text() const;
    [[nodiscard]] QString account_text() const;
    [[nodiscard]] QString call_duration_text() const;
    [[nodiscard]] bool incoming_call() const noexcept;
    [[nodiscard]] bool can_register() const noexcept;
    [[nodiscard]] bool can_call() const noexcept;
    [[nodiscard]] bool can_answer() const noexcept;
    [[nodiscard]] bool can_hangup() const noexcept;

    Q_INVOKABLE bool registerAccount(const QString &username, const QString &password,
                                     const QString &server, int server_port, int listen_port,
                                     const QString &transport);
    Q_INVOKABLE bool makeCall(const QString &destination);
    Q_INVOKABLE bool answerCall();
    Q_INVOKABLE bool hangupCall();
    Q_INVOKABLE bool resetSession();

    void on_state_changed(StateTransition transition) override;

  signals:
    void viewStateChanged();
    void callDurationChanged();

  private:
    [[nodiscard]] static bool valid_port(int port, bool allow_zero) noexcept;
    [[nodiscard]] CallState current_state() const noexcept;
    void rebuild_session(const SipConnectionSettings &settings);
    void refresh_from_manager();
    void set_local_error(QString error);
    void update_call_timer(CallState state);

    SipEngineFactory engine_factory_;
    std::unique_ptr<application::SipEngine> engine_;
    std::unique_ptr<application::CallManager> call_manager_;

    QString state_text_{QStringLiteral("Idle")};
    QString peer_;
    QString error_text_;
    QString account_text_;
    bool incoming_call_{false};
    bool can_register_{true};
    bool can_call_{false};
    bool can_answer_{false};
    bool can_hangup_{false};

    QElapsedTimer call_elapsed_;
    QTimer duration_timer_;
};

} // namespace voip::gui
