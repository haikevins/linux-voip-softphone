#include <QGuiApplication>
#include <QQuickView>
#include <QString>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication application{argc, argv};

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{QStringLiteral("qrc:/qt/qml/VoipSoftphone/Main.qml")});

    if (view.status() == QQuickView::Error) {
        return 1;
    }

    view.setTitle(QStringLiteral("Linux VoIP Softphone"));
    view.resize(480, 720);
    view.show();

    return application.exec();
}
