#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "protocol.hpp"

#include <QMainWindow>

class QComboBox;
class QPushButton;
class QSerialPort;
class QSpinBox;
class QTextEdit;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void togglePort();
    void onReadyRead();
    void onAutoToggled(bool on);
    void onAutoRandomToggled(bool on);
    void sendAllReal();
    void sendAllRealRandom();
    void hostSendStart();
    void hostSendStop();
    void clearLog();

private:
    void hostSendEnable(bool en);
    void sendSet(uint8_t id, bool en, float setVal);
    void sendReal(uint8_t id, float real);
    void logLine(const QString &s);

    QComboBox *portCombo_ = nullptr;
    QPushButton *btnOpen_ = nullptr;
    QPushButton *chkAutoReply_ = nullptr;
    QPushButton *chkAutoRandom_ = nullptr;
    QPushButton *btnOnceReal_ = nullptr;
    QSpinBox *spinCo2_ = nullptr;
    QSpinBox *spinTemp_ = nullptr;
    QSpinBox *spinHum_ = nullptr;
    QSpinBox *spinRpm_ = nullptr;
    QTextEdit *log_ = nullptr;
    QSerialPort *serial_ = nullptr;
    QTimer *timer_ = nullptr;
    QTimer *timerRandom_ = nullptr;
    proto::FrameParser parser_;
    bool deviceEnabled_ = false;
    int tick_ = 0;
};

#endif
