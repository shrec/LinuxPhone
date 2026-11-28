#include <QtTest>
#include <QTemporaryDir>
#include "status.h"

// Creates <root>/<name>/ with one file per key/value pair.
static void makeSupply(const QString &root, const QString &name, const QMap<QString, QByteArray> &files) {
    QVERIFY(QDir().mkpath(root + "/" + name));
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        QFile f(root + "/" + name + "/" + it.key());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(it.value());
    }
}

class TstStatus : public QObject {
    Q_OBJECT
private slots:
    void readsChargingBattery() {
        QTemporaryDir dir;
        makeSupply(dir.path(), "pm660-charger", {{"type", "USB\n"}, {"online", "1\n"}});
        makeSupply(dir.path(), "qcom-battery", {{"type", "Battery\n"}, {"capacity", "87\n"}, {"status", "Charging\n"}});
        const Battery b = readBattery(dir.path());
        QCOMPARE(b.percent, 87);
        QVERIFY(b.charging);
    }
    void fullIsNotCharging() {
        QTemporaryDir dir;
        makeSupply(dir.path(), "pm660-charger", {{"type", "USB\n"}});
        makeSupply(dir.path(), "qcom-battery", {{"type", "Battery\n"}, {"capacity", "100\n"}, {"status", "Full\n"}});
        const Battery b = readBattery(dir.path());
        QCOMPARE(b.percent, 100);
        QVERIFY(!b.charging);
    }
    void dischargingIsNotCharging() {
        QTemporaryDir dir;
        makeSupply(dir.path(), "bat", {{"type", "Battery\n"}, {"capacity", "42\n"}, {"status", "Discharging\n"}});
        const Battery b = readBattery(dir.path());
        QCOMPARE(b.percent, 42);
        QVERIFY(!b.charging);
    }
    void noBatteryEntry() {
        QTemporaryDir dir;
        makeSupply(dir.path(), "pm660-charger", {{"type", "USB\n"}, {"capacity", "55\n"}});
        const Battery b = readBattery(dir.path());
        QCOMPARE(b.percent, -1);
        QVERIFY(!b.charging);
    }
    void missingRoot() {
        QTemporaryDir dir;
        QCOMPARE(readBattery(dir.filePath("nope")).percent, -1);
    }
    void malformedCapacity_data() {
        QTest::addColumn<QByteArray>("capacity");
        QTest::newRow("text") << QByteArray("abc\n");
        QTest::newRow("empty") << QByteArray("");
        QTest::newRow("too high") << QByteArray("101\n");
        QTest::newRow("negative") << QByteArray("-3\n");
    }
    void malformedCapacity() {
        QFETCH(QByteArray, capacity);
        QTemporaryDir dir;
        makeSupply(dir.path(), "bat", {{"type", "Battery\n"}, {"capacity", capacity}, {"status", "Charging\n"}});
        const Battery b = readBattery(dir.path());
        QCOMPARE(b.percent, -1);
        QVERIFY(!b.charging);
    }
    void missingCapacityFile() {
        QTemporaryDir dir;
        makeSupply(dir.path(), "bat", {{"type", "Battery\n"}, {"status", "Charging\n"}});
        QCOMPARE(readBattery(dir.path()).percent, -1);
    }
    void signalBarsBoundaries_data() {
        QTest::addColumn<int>("dBm");
        QTest::addColumn<int>("bars");
        QTest::newRow("-55") << -55 << 4;
        QTest::newRow("-56") << -56 << 3;
        QTest::newRow("-66") << -66 << 3;
        QTest::newRow("-67") << -67 << 2;
        QTest::newRow("-77") << -77 << 2;
        QTest::newRow("-78") << -78 << 1;
        QTest::newRow("-88") << -88 << 1;
        QTest::newRow("-89") << -89 << 0;
    }
    void signalBarsBoundaries() {
        QFETCH(int, dBm);
        QFETCH(int, bars);
        QCOMPARE(signalBars(dBm), bars);
    }
};

QTEST_MAIN(TstStatus)
#include "tst_status.moc"
