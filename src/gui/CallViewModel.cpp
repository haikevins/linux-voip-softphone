#include "gui/CallViewModel.hpp"

#include "core/CallState.hpp"

#include <QMetaObject>

#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace voip::gui {
namespace {

[[nodiscard]] QString to_qstring(const std::string &value)
{
    return QString::fromStdString(value);
}

[[nodiscard]] QString to_qstring(const std::string_view value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

} // namespace

CallViewModel::CallViewModel(SipEngineFactory engine_factory, QObject *parent)
    : QObject(parent), engine_factory_(std::move(engine_factory))
{
    duration_timer_.setInterval(1000);
    duration_timer_.setSingleShot(false);
    connect(&duration_timer_, &QTimer::timeout, this, &CallViewModel::callDurationChanged);
}

QString CallViewModel::state_text() const
{
    return state_text_;
}

QString CallViewModel::peer() const
{
    return peer_;
}

QString CallViewModel::error_text() const
{
    return error_text_;
}

QString CallViewModel::account_text() const
{
    return account_text_;
}

QString CallViewModel::call_duration_text() const
{
    if (!call_elapsed_.isValid()) {
        return QStringLiteral("00:00");
    }

    const auto total_seconds = call_elapsed_.elapsed() / 1000;
    const auto minutes = total_seconds / 60;
    const auto seconds = total_seconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

bool CallViewModel::incoming_call() const noexcept
{
    return incoming_call_;
}

bool CallViewModel::can_register() const noexcept
{
    return can_register_;
}

bool CallViewModel::can_call() const noexcept
{
    return can_call_;
}

bool CallViewModel::can_answer() const noexcept
{
    return can_answer_;
}

bool CallViewModel::can_hangup() const noexcept
{
    return can_hangup_;
}

bool CallViewModel::registerAccount(const QString &username, const QString &password,
                                    const QString &server, const int server_port,
                                    const int listen_port, const QString &transport)
{
    if (!can_register_) {
        set_local_error(QStringLiteral("Finish or reset the current session before registering."));
        return false;
    }
    if (username.trimmed().isEmpty() || password.isEmpty() || server.trimmed().isEmpty()) {
        set_local_error(QStringLiteral("Username, password, and SIP server are required."));
        return false;
    }
    if (!valid_port(server_port, false) || !valid_port(listen_port, true)) {
        set_local_error(
            QStringLiteral("Server port must be 1-65535; listen port must be 0-65535."));
        return false;
    }

    const auto normalized_transport = transport.trimmed().toLower();
    if (normalized_transport != QStringLiteral("udp") &&
        normalized_transport != QStringLiteral("tcp")) {
        set_local_error(QStringLiteral("Transport must be UDP or TCP."));
        return false;
    }

    SipConnectionSettings settings;
    settings.username = username.trimmed().toStdString();
    settings.password = password.toStdString();
    settings.server = server.trimmed().toStdString();
    settings.server_port = static_cast<std::uint16_t>(server_port);
    settings.listen_port = static_cast<std::uint16_t>(listen_port);
    settings.use_tcp = normalized_transport == QStringLiteral("tcp");

    try {
        rebuild_session(settings);
        account_text_ = username.trimmed() + QStringLiteral("@") + server.trimmed();
        error_text_.clear();
        emit viewStateChanged();

        const auto accepted = call_manager_->register_account();
        refresh_from_manager();
        return accepted;
    } catch (const std::exception &error) {
        set_local_error(QString::fromUtf8(error.what()));
    } catch (...) {
        set_local_error(QStringLiteral("Unable to create the SIP session."));
    }

    return false;
}

bool CallViewModel::makeCall(const QString &destination)
{
    if (call_manager_ == nullptr) {
        set_local_error(QStringLiteral("Register the SIP account before placing a call."));
        return false;
    }

    const auto trimmed_destination = destination.trimmed();
    if (trimmed_destination.isEmpty()) {
        set_local_error(QStringLiteral("Enter a call destination."));
        return false;
    }

    error_text_.clear();
    const auto accepted = call_manager_->make_call(trimmed_destination.toStdString());
    refresh_from_manager();
    return accepted;
}

bool CallViewModel::answerCall()
{
    if (call_manager_ == nullptr || !incoming_call_) {
        return false;
    }

    error_text_.clear();
    const auto accepted = call_manager_->answer_call();
    refresh_from_manager();
    return accepted;
}

bool CallViewModel::hangupCall()
{
    if (call_manager_ == nullptr) {
        return false;
    }

    const auto accepted = call_manager_->hangup_call();
    refresh_from_manager();
    return accepted;
}

bool CallViewModel::resetSession()
{
    if (call_manager_ != nullptr && current_state() == CallState::Failed) {
        static_cast<void>(call_manager_->reset());
    }

    call_manager_.reset();
    engine_.reset();
    account_text_.clear();
    peer_.clear();
    error_text_.clear();
    state_text_ = QStringLiteral("Idle");
    incoming_call_ = false;
    can_register_ = true;
    can_call_ = false;
    can_answer_ = false;
    can_hangup_ = false;
    call_elapsed_.invalidate();
    duration_timer_.stop();

    emit viewStateChanged();
    emit callDurationChanged();
    return true;
}

void CallViewModel::on_state_changed(const StateTransition /*transition*/)
{
    QMetaObject::invokeMethod(this, [this]() { refresh_from_manager(); }, Qt::QueuedConnection);
}

bool CallViewModel::valid_port(const int port, const bool allow_zero) noexcept
{
    const auto minimum = allow_zero ? 0 : 1;
    return port >= minimum && port <= 65'535;
}

CallState CallViewModel::current_state() const noexcept
{
    return call_manager_ == nullptr ? CallState::Idle : call_manager_->state();
}

void CallViewModel::rebuild_session(const SipConnectionSettings &settings)
{
    call_manager_.reset();
    engine_.reset();

    engine_ = engine_factory_(settings);
    if (engine_ == nullptr) {
        throw std::runtime_error("SIP engine factory returned no engine");
    }
    call_manager_ = std::make_unique<application::CallManager>(*engine_, *this);
}

void CallViewModel::refresh_from_manager()
{
    const auto state = current_state();
    state_text_ = to_qstring(to_string(state));

    if (call_manager_ != nullptr) {
        const auto manager_peer = call_manager_->peer();
        peer_ = manager_peer.has_value() ? to_qstring(manager_peer.value()) : QString{};

        const auto manager_error = call_manager_->last_error();
        if (manager_error.has_value()) {
            error_text_ = to_qstring(manager_error.value());
        }
        incoming_call_ = call_manager_->incoming_call();
    } else {
        peer_.clear();
        incoming_call_ = false;
    }

    can_register_ = state == CallState::Idle || state == CallState::Failed;
    can_call_ = state == CallState::Ready;
    can_answer_ = state == CallState::Ringing && incoming_call_;
    can_hangup_ =
        state == CallState::Calling || state == CallState::Ringing || state == CallState::Connected;

    update_call_timer(state);
    emit viewStateChanged();
}

void CallViewModel::set_local_error(QString error)
{
    error_text_ = std::move(error);
    emit viewStateChanged();
}

void CallViewModel::update_call_timer(const CallState state)
{
    if (state == CallState::Connected) {
        if (!call_elapsed_.isValid()) {
            call_elapsed_.start();
            duration_timer_.start();
            emit callDurationChanged();
        }
        return;
    }

    if (call_elapsed_.isValid()) {
        call_elapsed_.invalidate();
        duration_timer_.stop();
        emit callDurationChanged();
    }
}

} // namespace voip::gui
