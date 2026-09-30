#include "gui/GuiViewModel.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include <cstdlib>
#include <string_view>

int main(int argc, char *argv[])
{
    const bool smoke_test = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (argc > 1 && !smoke_test) {
        return EXIT_FAILURE;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);

    voip::gui::GuiViewModel view_model;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("softphone"), &view_model);
    engine.load(QUrl(QStringLiteral("qrc:/gui/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        // Fail if the shell accidentally presents a live/registered backend.
        if (view_model.callActionsEnabled() ||
            engine.rootObjects().constFirst()->findChild<QObject *>(
                QStringLiteral("destinationField")) == nullptr) {
            return EXIT_FAILURE;
        }
        QTimer::singleShot(100, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
