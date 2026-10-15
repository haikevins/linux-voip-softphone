#include "gui/CallViewModel.hpp"

#ifdef VOIP_GUI_WITH_PJSIP
#include "pjsip/Pjsua2SipEngine.hpp"
#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"
#endif

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QString>
#include <QUrl>

#include <memory>
#include <stdexcept>
#include <utility>

namespace {

#ifndef VOIP_GUI_WITH_PJSIP
class UnavailableSipEngine final : public voip::application::SipEngine
{
  public:
    void set_observer(voip::application::SipEngineObserver *observer) noexcept override
    {
        static_cast<void>(observer);
    }

    void register_account() override
    {
        throw std::runtime_error(
            "PJSIP backend is not enabled; build with the qt-pjsip-debug preset");
    }

    void make_call(const std::string & /*destination*/) override
    {
        throw std::runtime_error("PJSIP backend is not enabled");
    }

    void answer_call() override
    {
        throw std::runtime_error("PJSIP backend is not enabled");
    }

    void hangup_call() override
    {
        throw std::runtime_error("PJSIP backend is not enabled");
    }
};
#endif

[[nodiscard]] std::unique_ptr<voip::application::SipEngine>
create_sip_engine(const voip::gui::SipConnectionSettings &settings)
{
#ifdef VOIP_GUI_WITH_PJSIP
    const auto transport =
        settings.use_tcp ? voip::pjsip::SipTransport::Tcp : voip::pjsip::SipTransport::Udp;

    voip::pjsip::SipEndpointConfig endpoint_config;
    endpoint_config.transport = transport;
    endpoint_config.listen_port = settings.listen_port;

    voip::pjsip::SipAccountConfig account_config;
    account_config.username = settings.username;
    account_config.password = settings.password;
    account_config.server = settings.server;
    account_config.server_port = settings.server_port;
    account_config.realm = settings.realm;
    account_config.transport = transport;

    return std::make_unique<voip::pjsip::Pjsua2SipEngine>(endpoint_config,
                                                          std::move(account_config));
#else
    static_cast<void>(settings);
    return std::make_unique<UnavailableSipEngine>();
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application{argc, argv};
    voip::gui::CallViewModel call_view_model{create_sip_engine};

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("callViewModel"), &call_view_model);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{QStringLiteral("qrc:/qt/qml/VoipSoftphone/Main.qml")});

    if (view.status() == QQuickView::Error) {
        return 1;
    }

    view.setTitle(QStringLiteral("Linux VoIP Softphone"));
    view.resize(480, 720);
    view.setMinimumSize(QSize{420, 640});
    view.show();

    return application.exec();
}
