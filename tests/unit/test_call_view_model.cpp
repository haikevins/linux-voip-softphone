#include "application/SipEngine.hpp"
#include "gui/CallViewModel.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QString>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

QCoreApplication &qt_application()
{
    static int argc = 1;
    static char app_name[] = "call-view-model-tests";
    static char *argv[] = {app_name, nullptr};
    static QCoreApplication application{argc, argv};
    return application;
}

void process_qt_events()
{
    static_cast<void>(qt_application());
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

class FakeSipEngine final : public voip::application::SipEngine
{
  public:
    void set_observer(voip::application::SipEngineObserver *observer) noexcept override
    {
        observer_ = observer;
    }

    void register_account() override
    {
        ++register_calls;
        if (throw_on_register) {
            throw std::runtime_error("registration failed in fake engine");
        }
    }

    void make_call(const std::string &destination) override
    {
        ++make_call_calls;
        last_destination = destination;
    }

    void answer_call() override
    {
        ++answer_calls;
    }

    void hangup_call() override
    {
        ++hangup_calls;
    }

    void registration_succeeded()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_registration_succeeded();
    }

    void incoming_call(std::string peer)
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_incoming_call(std::move(peer));
    }

    void remote_ringing()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_remote_ringing();
    }

    void call_connected()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_call_connected();
    }

    void call_ended()
    {
        ASSERT_NE(observer_, nullptr);
        observer_->on_call_ended();
    }

    bool throw_on_register{false};
    int register_calls{0};
    int make_call_calls{0};
    int answer_calls{0};
    int hangup_calls{0};
    std::string last_destination;

  private:
    voip::application::SipEngineObserver *observer_{nullptr};
};

struct ViewModelHarness
{
    voip::gui::SipConnectionSettings settings;
    FakeSipEngine *engine{nullptr};

    [[nodiscard]] voip::gui::SipEngineFactory factory()
    {
        return [this](const voip::gui::SipConnectionSettings &new_settings) {
            settings = new_settings;
            auto fake_engine = std::make_unique<FakeSipEngine>();
            engine = fake_engine.get();
            return fake_engine;
        };
    }
};

void register_ready(voip::gui::CallViewModel &view_model, ViewModelHarness &harness)
{
    ASSERT_TRUE(view_model.registerAccount(QStringLiteral("1001"), QStringLiteral("test-password"),
                                           QStringLiteral("127.0.0.1"), 5060, 0,
                                           QStringLiteral("udp")));
    ASSERT_NE(harness.engine, nullptr);
    harness.engine->registration_succeeded();
    process_qt_events();
    ASSERT_EQ(view_model.state_text().toStdString(), "Ready");
}

} // namespace

TEST(CallViewModelTest, ValidatesRegistrationInputsBeforeCreatingEngine)
{
    static_cast<void>(qt_application());
    ViewModelHarness harness;
    voip::gui::CallViewModel view_model{harness.factory()};

    EXPECT_FALSE(view_model.registerAccount(QString{}, QStringLiteral("test-password"),
                                            QStringLiteral("127.0.0.1"), 5060, 0,
                                            QStringLiteral("udp")));
    EXPECT_EQ(harness.engine, nullptr);
    EXPECT_FALSE(view_model.error_text().isEmpty());

    EXPECT_FALSE(view_model.registerAccount(QStringLiteral("1001"), QStringLiteral("test-password"),
                                            QStringLiteral("127.0.0.1"), 0, 0,
                                            QStringLiteral("udp")));
    EXPECT_EQ(harness.engine, nullptr);

    EXPECT_FALSE(view_model.registerAccount(QStringLiteral("1001"), QStringLiteral("test-password"),
                                            QStringLiteral("127.0.0.1"), 5060, 0,
                                            QStringLiteral("tls")));
    EXPECT_EQ(harness.engine, nullptr);
}

TEST(CallViewModelTest, MapsRegistrationSettingsAndBecomesReady)
{
    static_cast<void>(qt_application());
    ViewModelHarness harness;
    voip::gui::CallViewModel view_model{harness.factory()};

    EXPECT_TRUE(view_model.registerAccount(
        QStringLiteral(" 1001 "), QStringLiteral("test-password"), QStringLiteral(" pbx.local "),
        5070, 5090, QStringLiteral(" TCP ")));
    ASSERT_NE(harness.engine, nullptr);
    EXPECT_EQ(harness.engine->register_calls, 1);
    EXPECT_EQ(harness.settings.username, "1001");
    EXPECT_EQ(harness.settings.password, "test-password");
    EXPECT_EQ(harness.settings.server, "pbx.local");
    EXPECT_EQ(harness.settings.server_port, 5070U);
    EXPECT_EQ(harness.settings.listen_port, 5090U);
    EXPECT_TRUE(harness.settings.use_tcp);
    EXPECT_EQ(view_model.state_text().toStdString(), "Registering");
    EXPECT_EQ(view_model.account_text().toStdString(), "1001@pbx.local");
    EXPECT_FALSE(view_model.can_call());

    harness.engine->registration_succeeded();
    process_qt_events();

    EXPECT_EQ(view_model.state_text().toStdString(), "Ready");
    EXPECT_TRUE(view_model.can_call());
    EXPECT_FALSE(view_model.can_register());
}

TEST(CallViewModelTest, DrivesOutgoingCallStateFromEngineCallbacks)
{
    static_cast<void>(qt_application());
    ViewModelHarness harness;
    voip::gui::CallViewModel view_model{harness.factory()};
    register_ready(view_model, harness);

    ASSERT_TRUE(view_model.makeCall(QStringLiteral(" 1002 ")));
    ASSERT_NE(harness.engine, nullptr);
    EXPECT_EQ(harness.engine->make_call_calls, 1);
    EXPECT_EQ(harness.engine->last_destination, "1002");
    EXPECT_EQ(view_model.state_text().toStdString(), "Calling");
    EXPECT_EQ(view_model.peer().toStdString(), "1002");
    EXPECT_TRUE(view_model.can_hangup());

    harness.engine->remote_ringing();
    process_qt_events();
    EXPECT_EQ(view_model.state_text().toStdString(), "Ringing");
    EXPECT_FALSE(view_model.can_answer());

    harness.engine->call_connected();
    process_qt_events();
    EXPECT_EQ(view_model.state_text().toStdString(), "Connected");

    EXPECT_TRUE(view_model.hangupCall());
    EXPECT_EQ(harness.engine->hangup_calls, 1);
    EXPECT_EQ(view_model.state_text().toStdString(), "Terminating");

    harness.engine->call_ended();
    process_qt_events();
    EXPECT_EQ(view_model.state_text().toStdString(), "Ready");
    EXPECT_TRUE(view_model.peer().isEmpty());
}

TEST(CallViewModelTest, ExposesIncomingCallActions)
{
    static_cast<void>(qt_application());
    ViewModelHarness harness;
    voip::gui::CallViewModel view_model{harness.factory()};
    register_ready(view_model, harness);

    ASSERT_NE(harness.engine, nullptr);
    harness.engine->incoming_call("1003");
    process_qt_events();

    EXPECT_EQ(view_model.state_text().toStdString(), "Ringing");
    EXPECT_EQ(view_model.peer().toStdString(), "1003");
    EXPECT_TRUE(view_model.incoming_call());
    EXPECT_TRUE(view_model.can_answer());

    EXPECT_TRUE(view_model.answerCall());
    EXPECT_EQ(harness.engine->answer_calls, 1);

    harness.engine->call_connected();
    process_qt_events();
    EXPECT_EQ(view_model.state_text().toStdString(), "Connected");
    EXPECT_FALSE(view_model.can_answer());
}

TEST(CallViewModelTest, SurfacesBackendFailureAndCanResetSession)
{
    static_cast<void>(qt_application());
    ViewModelHarness harness;
    auto factory = [&harness](const voip::gui::SipConnectionSettings &settings) {
        harness.settings = settings;
        auto fake_engine = std::make_unique<FakeSipEngine>();
        fake_engine->throw_on_register = true;
        harness.engine = fake_engine.get();
        return fake_engine;
    };
    voip::gui::CallViewModel view_model{factory};

    EXPECT_FALSE(view_model.registerAccount(QStringLiteral("1001"), QStringLiteral("test-password"),
                                            QStringLiteral("127.0.0.1"), 5060, 0,
                                            QStringLiteral("udp")));
    EXPECT_EQ(view_model.state_text().toStdString(), "Failed");
    EXPECT_NE(view_model.error_text().toStdString().find("registration failed"), std::string::npos);
    EXPECT_TRUE(view_model.can_register());

    EXPECT_TRUE(view_model.resetSession());
    EXPECT_EQ(view_model.state_text().toStdString(), "Idle");
    EXPECT_TRUE(view_model.account_text().isEmpty());
    EXPECT_TRUE(view_model.error_text().isEmpty());
    EXPECT_TRUE(view_model.can_register());
}
