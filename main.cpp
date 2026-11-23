#include "wrapper/common/appCoreWrap.h"
#include "wrapper/common/qmlNetworkWrap.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_ANDROID
    // The controls' style the desktop uses; Android's default (Material) moves a field's placeholder.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
#endif
    QGuiApplication app(argc, argv);
#ifdef Q_OS_ANDROID
    // Portrait only: the phone layout is made for it.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        activity.callMethod<void>("setRequestedOrientation", "(I)V", jint(1));   // SCREEN_ORIENTATION_PORTRAIT
    });
#endif
    QCoreApplication::setOrganizationName("ProjectManga");
    QCoreApplication::setApplicationName("ProjectManga");
    QCoreApplication::setApplicationVersion(PROJECTMANGA_VERSION);
    // The desktop matches the window to its launcher entry and icon by this id.
    QGuiApplication::setDesktopFileName("io.github.chandandasgit.ProjectManga");
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon.svg")));

    // ~/.local/share/ProjectManga and ~/.cache/ProjectManga (or wherever XDG_DATA_HOME and
    // XDG_CACHE_HOME point).
    const QString dataFolder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/ProjectManga";
    const QString cacheFolder = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/ProjectManga";

    AppCoreWrap core(dataFolder, cacheFolder);
    QmlNetworkWrap imageNetwork(cacheFolder + "/images");
    QmlNetworkWrap::quietImageLoadErrors();

    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&imageNetwork);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ProjectManga", "Main");
    core.start();
#ifdef Q_OS_ANDROID
    // Android may stop the app at any time once it is in the background: sync before that, and
    // bring in what the other devices did meanwhile on coming back.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &core, [&core](Qt::ApplicationState state) {
        core.applicationActiveChanged(state == Qt::ApplicationActive);
    });
#endif

    return QGuiApplication::exec();
}
