#include "mainwindow.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QRandomGenerator>

static QString idName(uint8_t id)
{
    switch (id) {
    case proto::ID_CO2:
        return QStringLiteral("CO2");
    case proto::ID_TEMP:
        return QStringLiteral("TEMP");
    case proto::ID_HUM:
        return QStringLiteral("HUM");
    case proto::ID_RPM:
        return QStringLiteral("RPM");
    default:
        return QStringLiteral("0x%1").arg(id, 2, 16, QChar('0'));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto *root = new QWidget(this);
    setCentralWidget(root);
    auto *layout = new QVBoxLayout(root);

    auto *portRow = new QHBoxLayout();
    portCombo_ = new QComboBox();
    refreshPorts();
    auto *btnRefresh = new QPushButton(QStringLiteral("刷新端口"));
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    btnOpen_ = new QPushButton(QStringLiteral("打开"));
    connect(btnOpen_, &QPushButton::clicked, this, &MainWindow::togglePort);
    portRow->addWidget(new QLabel(QStringLiteral("串口:")));
    portRow->addWidget(portCombo_, 1);
    portRow->addWidget(btnRefresh);
    portRow->addWidget(btnOpen_);
    layout->addLayout(portRow);

    auto *roleBox = new QGroupBox(QStringLiteral("角色说明"));
    auto *roleLay = new QVBoxLayout(roleBox);
    roleLay->addWidget(new QLabel(
        QStringLiteral("本 Demo 模拟【设备】：接收屏(EVENT 启停)，按周期回 PERIOD_EVENT 实时值。\n"
                       "ESP 工程为【屏】：GPIO37/38 UART，点主页启动/停止发 EVENT。")));
    layout->addWidget(roleBox);

    auto *simBox = new QGroupBox(QStringLiteral("模拟设备"));
    auto *simLay = new QVBoxLayout(simBox);
    auto *simRow1 = new QHBoxLayout();
    chkAutoReply_ = new QPushButton(QStringLiteral("设备已使能 (自动 1s 上报)"));
    chkAutoReply_->setCheckable(true);
    chkAutoReply_->setEnabled(false);
    connect(chkAutoReply_, &QPushButton::toggled, this, &MainWindow::onAutoToggled);
    btnOnceReal_ = new QPushButton(QStringLiteral("立即发一轮实时值"));
    connect(btnOnceReal_, &QPushButton::clicked, this, &MainWindow::sendAllReal);
    simRow1->addWidget(chkAutoReply_);
    simRow1->addWidget(btnOnceReal_);
    simLay->addLayout(simRow1);

    auto *simRow2 = new QHBoxLayout();
    chkAutoRandom_ = new QPushButton(QStringLiteral("设备已使能 (自动 1s 上报，随机值)"));
    chkAutoRandom_->setCheckable(true);
    chkAutoRandom_->setEnabled(false);
    connect(chkAutoRandom_, &QPushButton::toggled, this, &MainWindow::onAutoRandomToggled);
    simRow2->addWidget(chkAutoRandom_);
    simLay->addLayout(simRow2);
    layout->addWidget(simBox);

    auto *hostBox = new QGroupBox(QStringLiteral("模拟屏(可选，无 ESP 时自测)"));
    auto *hostLay = new QHBoxLayout(hostBox);
    auto *btnHostStart = new QPushButton(QStringLiteral("发启动 EVENT×4"));
    auto *btnHostStop = new QPushButton(QStringLiteral("发停止 EVENT×4"));
    connect(btnHostStart, &QPushButton::clicked, this, &MainWindow::hostSendStart);
    connect(btnHostStop, &QPushButton::clicked, this, &MainWindow::hostSendStop);
    hostLay->addWidget(btnHostStart);
    hostLay->addWidget(btnHostStop);
    layout->addWidget(hostBox);

    auto *form = new QFormLayout();
    spinCo2_ = new QSpinBox();
    spinCo2_->setRange(0, 100);
    spinCo2_->setValue(40);
    spinTemp_ = new QSpinBox();
    spinTemp_->setRange(0, 60);
    spinTemp_->setValue(25);
    spinHum_ = new QSpinBox();
    spinHum_->setRange(0, 100);
    spinHum_->setValue(55);
    spinRpm_ = new QSpinBox();
    spinRpm_->setRange(0, 500);
    spinRpm_->setValue(120);
    form->addRow(QStringLiteral("CO2 实时"), spinCo2_);
    form->addRow(QStringLiteral("温度"), spinTemp_);
    form->addRow(QStringLiteral("湿度"), spinHum_);
    form->addRow(QStringLiteral("转速"), spinRpm_);
    layout->addLayout(form);

    auto *logRow = new QHBoxLayout();
    logRow->addWidget(new QLabel(QStringLiteral("日志")));
    logRow->addStretch();
    auto *btnClearLog = new QPushButton(QStringLiteral("清除"));
    connect(btnClearLog, &QPushButton::clicked, this, &MainWindow::clearLog);
    logRow->addWidget(btnClearLog);
    layout->addLayout(logRow);

    log_ = new QTextEdit();
    log_->setReadOnly(true);
    layout->addWidget(log_, 1);

    serial_ = new QSerialPort(this);
    connect(serial_, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &MainWindow::sendAllReal);

    timerRandom_ = new QTimer(this);
    timerRandom_->setInterval(1000);
    connect(timerRandom_, &QTimer::timeout, this, &MainWindow::sendAllRealRandom);

    tick_ = 0;
    setWindowTitle(QStringLiteral("UART 设备模拟 — ESP32 屏工程联调"));
    resize(640, 520);
}

void MainWindow::refreshPorts()
{
    const QString cur = portCombo_->currentText();
    portCombo_->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        portCombo_->addItem(info.portName() + QStringLiteral(" ") + info.description(), info.portName());
    }
    const int idx = portCombo_->findData(cur);
    if (idx >= 0) {
        portCombo_->setCurrentIndex(idx);
    }
}

void MainWindow::togglePort()
{
    if (serial_->isOpen()) {
        serial_->close();
        btnOpen_->setText(QStringLiteral("打开"));
        chkAutoReply_->setChecked(false);
        chkAutoRandom_->setChecked(false);
        chkAutoReply_->setEnabled(false);
        chkAutoRandom_->setEnabled(false);
        logLine(QStringLiteral("串口已关闭"));
        return;
    }
    const QString port = portCombo_->currentData().toString();
    if (port.isEmpty()) {
        logLine(QStringLiteral("无可用串口"));
        return;
    }
    serial_->setPortName(port);
    serial_->setBaudRate(115200);
    serial_->setDataBits(QSerialPort::Data8);
    serial_->setParity(QSerialPort::NoParity);
    serial_->setStopBits(QSerialPort::OneStop);
    serial_->setFlowControl(QSerialPort::NoFlowControl);
    if (!serial_->open(QIODevice::ReadWrite)) {
        logLine(QStringLiteral("打开失败: %1").arg(serial_->errorString()));
        return;
    }
    btnOpen_->setText(QStringLiteral("关闭"));
    chkAutoReply_->setEnabled(true);
    chkAutoRandom_->setEnabled(true);
    parser_.reset();
    logLine(QStringLiteral("已打开 %1 @115200").arg(port));
}

void MainWindow::onReadyRead()
{
    const QByteArray data = serial_->readAll();
    QList<proto::ParsedFrame> frames;
    parser_.feed(data, frames);
    for (const proto::ParsedFrame &f : frames) {
        logLine(QStringLiteral("RX id=%1 cat=0x%2 len=%3")
                    .arg(idName(f.infoId))
                    .arg(f.infoCat, 2, 16, QChar('0'))
                    .arg(f.payload.size()));
        if (f.infoCat == proto::CAT_EVENT && f.payload.size() == 8) {
            const bool en = f.payload[0] != 0;
            logLine(QStringLiteral("  EVENT 启停 en=%1").arg(en ? 1 : 0));
            deviceEnabled_ = en;
            chkAutoReply_->blockSignals(true);
            chkAutoRandom_->blockSignals(true);
            chkAutoReply_->setChecked(en);
            chkAutoRandom_->setChecked(false);
            chkAutoReply_->blockSignals(false);
            chkAutoRandom_->blockSignals(false);
            timer_->stop();
            timerRandom_->stop();
            if (en) {
                timer_->start();
                logLine(QStringLiteral("屏 EVENT：开始 1s 周期上报(固定值)"));
            } else {
                logLine(QStringLiteral("屏 EVENT：停止周期上报"));
            }
        }
    }
}

void MainWindow::onAutoToggled(bool on)
{
    deviceEnabled_ = on;
    if (on) {
        chkAutoRandom_->blockSignals(true);
        chkAutoRandom_->setChecked(false);
        chkAutoRandom_->blockSignals(false);
        timerRandom_->stop();
        timer_->start();
        logLine(QStringLiteral("开始 1s 周期上报实时值(固定)"));
    } else {
        timer_->stop();
        logLine(QStringLiteral("停止周期上报(固定)"));
    }
}

void MainWindow::onAutoRandomToggled(bool on)
{
    deviceEnabled_ = on;
    if (on) {
        chkAutoReply_->blockSignals(true);
        chkAutoReply_->setChecked(false);
        chkAutoReply_->blockSignals(false);
        timer_->stop();
        timerRandom_->start();
        logLine(QStringLiteral("开始 1s 周期上报实时值(随机)"));
    } else {
        timerRandom_->stop();
        logLine(QStringLiteral("停止周期上报(随机)"));
    }
}

void MainWindow::sendAllReal()
{
    if (!serial_->isOpen()) {
        return;
    }
    tick_++;
    const float co2 = static_cast<float>(spinCo2_->value());
    const float temp = static_cast<float>(spinTemp_->value());
    const float hum = static_cast<float>(spinHum_->value());
    const float rpm = static_cast<float>(spinRpm_->value() + (tick_ % 5));

    sendReal(proto::ID_CO2, co2);
    sendReal(proto::ID_TEMP, temp);
    sendReal(proto::ID_HUM, hum);
    sendReal(proto::ID_RPM, rpm);
}

void MainWindow::sendAllRealRandom()
{
    if (!serial_->isOpen()) {
        return;
    }
    auto *rng = QRandomGenerator::global();
    const auto randF = [](QRandomGenerator *r, int lo, int hi) -> float {
        if (hi <= lo) {
            return static_cast<float>(lo);
        }
        return static_cast<float>(lo) + r->generateDouble() * static_cast<float>(hi - lo);
    };

    const float co2 = randF(rng, spinCo2_->minimum(), spinCo2_->maximum());
    const float temp = randF(rng, spinTemp_->minimum(), spinTemp_->maximum());
    const float hum = randF(rng, spinHum_->minimum(), spinHum_->maximum());
    const float rpm = randF(rng, spinRpm_->minimum(), spinRpm_->maximum());

    sendReal(proto::ID_CO2, co2);
    sendReal(proto::ID_TEMP, temp);
    sendReal(proto::ID_HUM, hum);
    sendReal(proto::ID_RPM, rpm);
}

void MainWindow::hostSendStart()
{
    hostSendEnable(true);
}

void MainWindow::hostSendStop()
{
    hostSendEnable(false);
}

void MainWindow::hostSendEnable(bool en)
{
    if (!serial_->isOpen()) {
        return;
    }
    const float sets[] = {500.0f, 25.0f, 60.0f, 100.0f};
    const uint8_t ids[] = {proto::ID_CO2, proto::ID_TEMP, proto::ID_HUM, proto::ID_RPM};
    for (int i = 0; i < 4; ++i) {
        sendSet(ids[i], en, sets[i]);
    }
    logLine(en ? QStringLiteral("TX 模拟屏: 启动 EVENT×4") : QStringLiteral("TX 模拟屏: 停止 EVENT×4"));
}

void MainWindow::sendSet(uint8_t id, bool en, float setVal)
{
    const QByteArray pl = proto::packSet(en, setVal);
    const QByteArray frame = proto::buildFrame(id, proto::CAT_EVENT, pl);
    serial_->write(frame);
}

void MainWindow::sendReal(uint8_t id, float real)
{
    const QByteArray pl = proto::packReal(real);
    const QByteArray frame = proto::buildFrame(id, proto::CAT_PERIOD_EVENT, pl);
    serial_->write(frame);
    logLine(QStringLiteral("TX REAL %1 = %2").arg(idName(id)).arg(real));
}

void MainWindow::logLine(const QString &s)
{
    log_->append(s);
}

void MainWindow::clearLog()
{
    log_->clear();
}
