#include "gui/GuiViewModel.hpp"
#include "gui/QtRegistrationBackend.hpp"

#ifdef VOIP_GUI_WITH_PJSIP
#include "gui/PjsipRegistrationSession.hpp"
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef VOIP_GUI_WITH_PJSIP
namespace {

[[nodiscard]] bool parse_port(const std::string_view value, std::uint16_t &port)
{
    unsigned int parsed = 0;
    const auto [last, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || last != value.data() + value.size() || parsed == 0U ||
        parsed > 65535U) {
        return false;
    }
    port = static_cast<std::uint16_t>(parsed);
    return true;
}

} // namespace
#endif

int main(int argc, char *argv[])
{
    const bool smoke_test = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    const bool sip_register =
        (argc == 4 || argc == 5) && std::string_view(argv[1]) == "--sip-register";
    if ((argc > 1 && !smoke_test && !sip_register) || (smoke_test && sip_register)) {
        std::cerr << "Usage: voip-qt-gui [--smoke-test | --sip-register USER SERVER [PORT]]\n";
        return EXIT_FAILURE;
    }

#ifndef VOIP_GUI_WITH_PJSIP
    if (sip_register) {
        std::cerr << "This build has no PJSIP integration; use the qt-pjsip-gui-debug preset.\n";
        return EXIT_FAILURE;
    }
#else
    voip::pjsip::SipAccountConfig account;
    voip::pjsip::SipEndpointConfig endpoint;
    if (sip_register) {
        account.username = argv[2];
        account.server = argv[3];
        if (argc == 5 && !parse_port(argv[4], account.server_port)) {
            std::cerr << "Invalid SIP server port.\n";
            return EXIT_FAILURE;
        }
        // Password is accepted only via standard input, never argv or QML.
        // Do not print PJSIP exceptions because they may include credentials.
        if (!std::getline(std::cin, account.password) || account.password.empty()) {
            std::cerr << "A SIP password must be supplied on standard input.\n";
            return EXIT_FAILURE;
        }
        try {
            account.validate();
        } catch (const std::exception &) {
            std::cerr << "SIP account parameters are invalid.\n";
            return EXIT_FAILURE;
        }
    }
#endif

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);

    // Declaration order is deliberate: the QML engine goes away first, then
    // ViewModel/CallManager, then the adapter joins its SIP worker thread.
#ifdef VOIP_GUI_WITH_PJSIP
    std::unique_ptr<voip::gui::QtRegistrationBackend> backend;
#endif
    std::unique_ptr<voip::gui::GuiViewModel> view_model;
#ifdef VOIP_GUI_WITH_PJSIP
    if (sip_register) {
        auto factory =
            [endpoint,
             account = std::move(account)]() -> std::unique_ptr<voip::gui::RegistrationSession> {
            return std::make_unique<voip::gui::PjsipRegistrationSession>(endpoint, account);
        };
        backend = std::make_unique<voip::gui::QtRegistrationBackend>(std::move(factory), true);
        view_model = std::make_unique<voip::gui::GuiViewModel>(*backend);
        QObject::connect(backend.get(), &voip::gui::QtRegistrationBackend::registrationFinished,
                         view_model.get(), [model = view_model.get()](const bool registered) {
                             if (registered) {
                                 static_cast<void>(model->manager().on_registration_succeeded());
                             } else {
                                 static_cast<void>(model->manager().on_registration_failed(
                                     "SIP registration failed or timed out"));
                             }
                         });
        QObject::connect(backend.get(), &voip::gui::QtRegistrationBackend::incomingCall,
                         view_model.get(), [model = view_model.get()](const QString &peer) {
                             static_cast<void>(
                                 model->manager().on_incoming_call(peer.toStdString()));
                         });
        QObject::connect(backend.get(), &voip::gui::QtRegistrationBackend::callRinging,
                         view_model.get(), [model = view_model.get()] {
                             static_cast<void>(model->manager().on_remote_ringing());
                         });
        QObject::connect(backend.get(), &voip::gui::QtRegistrationBackend::callConnected,
                         view_model.get(), [model = view_model.get()] {
                             static_cast<void>(model->manager().on_call_connected());
                         });
        QObject::connect(
            backend.get(), &voip::gui::QtRegistrationBackend::callEnded, view_model.get(),
            [model = view_model.get()] { static_cast<void>(model->manager().on_call_ended()); });
        QObject::connect(backend.get(), &voip::gui::QtRegistrationBackend::callFailed,
                         view_model.get(), [model = view_model.get()] {
                             static_cast<void>(model->manager().on_call_failed(
                                 "Call failed, timed out or media did not connect"));
                         });
    } else {
        view_model = std::make_unique<voip::gui::GuiViewModel>();
    }
#else
    view_model = std::make_unique<voip::gui::GuiViewModel>();
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("softphone"), view_model.get());
    engine.load(QUrl(QStringLiteral("qrc:/gui/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        // No SIP adapter is ever created in the smoke-test mode.
        if (view_model->callActionsEnabled() || view_model->registrationActionsEnabled() ||
            engine.rootObjects().constFirst()->findChild<QObject *>(
                QStringLiteral("destinationField")) == nullptr) {
            return EXIT_FAILURE;
        }
        QTimer::singleShot(100, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
