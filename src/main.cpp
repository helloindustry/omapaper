// taskpaper — a plain-text outliner for tasks, in the style of TaskPaper.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <cstdio>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"

static const char *usage = "Usage: taskpaper [FILE]\n\n"
    "Opens FILE, a TaskPaper (.taskpaper or .txt) outline, or the last file you had open. Edits are saved\n"
    "as you type. taskpaper is a graphical app and keeps running until its window is closed.\n\n"
    "Options:\n"
    "  -h, --help     Show this message and exit\n"
    "  -v, --version  Show the version and exit\n";

int main(int argc, char *argv[]) {
    // Answered before any window machinery starts, so these work with no display.
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-h" || arg == "--help") {
            fputs(usage, stdout);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            puts("taskpaper 0.1.0");
            return 0;
        }
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName("taskpaper");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("taskpaper");
    app.setWindowIcon(QIcon::fromTheme("taskpaper"));

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    // Ctrl+N starts another window with "--new": a blank, untitled document.
    const bool blank = app.arguments().value(1) == "--new";
    Backend backend(&theme, blank ? QString() : app.arguments().value(1));
    backend.setStartBlank(blank);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::saveOnQuit);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
