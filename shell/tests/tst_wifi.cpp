#include <QtTest>
#include "wifi.h"

static Network net(const QString &name, int signal100dBm, bool connected = false) {
    Network n;
    n.path = "/net/connman/iwd/0/4/" + name;
    n.name = name;
    n.type = "psk";
    n.signal100dBm = signal100dBm;
    n.connected = connected;
    return n;
}

static QStringList names(const QList<Network> &list) {
    QStringList out;
    for (const Network &n : list)
        out << n.name;
    return out;
}

class TstWifi : public QObject {
    Q_OBJECT
private slots:
    void strongestFirst() {
        const QList<Network> sorted = sortNetworks({net("weak", -8000), net("strong", -3000), net("mid", -6000)});
        QCOMPARE(names(sorted), QStringList({"strong", "mid", "weak"}));
    }
    void connectedBeatsStronger() {
        const QList<Network> sorted = sortNetworks({net("strong", -3000), net("home", -7000, true), net("mid", -5000)});
        QCOMPARE(names(sorted), QStringList({"home", "strong", "mid"}));
    }
    void equalSignalSortsByName() {
        const QList<Network> sorted = sortNetworks({net("b", -5000), net("c", -5000), net("a", -5000)});
        QCOMPARE(names(sorted), QStringList({"a", "b", "c"}));
    }
    void emptyAndSingle() {
        QVERIFY(sortNetworks({}).isEmpty());
        QCOMPARE(names(sortNetworks({net("only", -4000)})), QStringList({"only"}));
    }
    void keepsFields() {
        Network n = net("x", -4200);
        n.known = true;
        n.type = "open";
        const QList<Network> sorted = sortNetworks({n});
        QCOMPARE(sorted.first().path, n.path);
        QCOMPARE(sorted.first().signal100dBm, -4200);
        QVERIFY(sorted.first().known);
        QCOMPARE(sorted.first().type, QString("open"));
    }
    void isSecured_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<bool>("secured");
        QTest::newRow("open") << "open" << false;
        QTest::newRow("psk") << "psk" << true;
        QTest::newRow("8021x") << "8021x" << true;
        QTest::newRow("wep") << "wep" << true;
    }
    void isSecured() {
        QFETCH(QString, type);
        QFETCH(bool, secured);
        QCOMPARE(::isSecured(type), secured);
    }
};

QTEST_MAIN(TstWifi)
#include "tst_wifi.moc"
