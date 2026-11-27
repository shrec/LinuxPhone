#include <QtTest>
#include <QTemporaryDir>
#include "desktopentry.h"

static void write(const QString &path, const QByteArray &text) {
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(text);
}

class TstDesktopEntry : public QObject {
    Q_OBJECT
private slots:
    void parsesApplication() {
        auto e = DesktopEntry::parse("[Desktop Entry]\nType=Application\nName=Foot\nExec=foot\nIcon=foot\n"
                                     "StartupWMClass=FootTerm\nTerminal=false\n", "foot", "en_US");
        QVERIFY(e);
        QCOMPARE(e->id, QString("foot"));
        QCOMPARE(e->name, QString("Foot"));
        QCOMPARE(e->icon, QString("foot"));
        QCOMPARE(e->exec, QString("foot"));
        QCOMPARE(e->wmClass, QString("FootTerm"));
        QVERIFY(!e->terminal);
    }
    void skipsWhatALauncherMustNotShow_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("NoDisplay") << "[Desktop Entry]\nType=Application\nName=A\nExec=a\nNoDisplay=true\n";
        QTest::newRow("Hidden") << "[Desktop Entry]\nType=Application\nName=A\nExec=a\nHidden=true\n";
        QTest::newRow("Link") << "[Desktop Entry]\nType=Link\nName=A\nURL=https://x\n";
        QTest::newRow("no Exec") << "[Desktop Entry]\nType=Application\nName=A\n";
        QTest::newRow("no Name") << "[Desktop Entry]\nType=Application\nExec=a\n";
    }
    void skipsWhatALauncherMustNotShow() {
        QFETCH(QString, text);
        QVERIFY(!DesktopEntry::parse(text, "a", "en_US"));
    }
    void ignoresOtherGroupsAndComments() {
        auto e = DesktopEntry::parse("# c\n[Desktop Entry]\nType=Application\nName=Real\nExec=real\n"
                                     "[Desktop Action new]\nName=Fake\nExec=fake\n", "r", "en_US");
        QVERIFY(e);
        QCOMPARE(e->name, QString("Real"));
        QCOMPARE(e->exec, QString("real"));
    }
    void prefersLocalizedName() {
        const QString t = "[Desktop Entry]\nType=Application\nName=Calculator\nName[ka]=კალკულატორი\nExec=c\n";
        QCOMPARE(DesktopEntry::parse(t, "c", "ka_GE.UTF-8")->name, QString("კალკულატორი"));
        QCOMPARE(DesktopEntry::parse(t, "c", "en_US")->name, QString("Calculator"));
    }
    void readsTerminalFlag() {
        QVERIFY(DesktopEntry::parse("[Desktop Entry]\nType=Application\nName=b\nExec=btop\nTerminal=true\n",
                                    "btop", "C")->terminal);
    }
    void commandLine_data() {
        QTest::addColumn<QString>("exec");
        QTest::addColumn<QStringList>("argv");
        QTest::newRow("plain") << "foot" << QStringList{"foot"};
        QTest::newRow("field codes dropped") << "firefox-esr %u" << QStringList{"firefox-esr"};
        QTest::newRow("several codes") << "app --new %F %i %c" << QStringList{"app", "--new"};
        QTest::newRow("quoted") << "sh -c \"echo hi there\"" << QStringList{"sh", "-c", "echo hi there"};
        QTest::newRow("escaped quote") << "a \"say \\\"x\\\"\"" << QStringList{"a", "say \"x\""};
        QTest::newRow("percent") << "printf 100%%" << QStringList{"printf", "100%"};
        QTest::newRow("extra spaces") << "  a   b  " << QStringList{"a", "b"};
        QTest::newRow("empty quoted arg") << "a \"\"" << QStringList{"a", ""};
    }
    void commandLine() {
        QFETCH(QString, exec);
        QFETCH(QStringList, argv);
        QCOMPARE(DesktopEntry::commandLine(exec), argv);
    }
    void scanSortsAndEarlierDirShadowsLater() {
        QTemporaryDir user, sys;
        write(sys.filePath("b.desktop"), "[Desktop Entry]\nType=Application\nName=Bravo\nExec=b\n");
        write(sys.filePath("a.desktop"), "[Desktop Entry]\nType=Application\nName=alpha\nExec=a\n");
        write(sys.filePath("gone.desktop"), "[Desktop Entry]\nType=Application\nName=Gone\nExec=g\n");
        write(user.filePath("gone.desktop"), "[Desktop Entry]\nType=Application\nName=Gone\nExec=g\nHidden=true\n");
        write(sys.filePath("notes.txt"), "not a desktop file");
        const auto apps = DesktopEntry::scan({user.path(), sys.path()}, "C");
        QCOMPARE(apps.size(), 2);
        QCOMPARE(apps[0].name, QString("alpha"));
        QCOMPARE(apps[1].name, QString("Bravo"));
    }
};

QTEST_MAIN(TstDesktopEntry)
#include "tst_desktopentry.moc"
