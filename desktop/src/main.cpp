#include <guardian/desktop/system_metrics_view_model.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char* argv[]) {
    QGuiApplication application{argc, argv};
    application.setApplicationName(QStringLiteral("AI PC Guardian"));
    application.setOrganizationName(QStringLiteral("AI PC Guardian"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("GuardianApp", "Main");

    return application.exec();
}
