#pragma once

#include <QByteArray>
#include <QList>
#include <cstdint>
#include <cstring>

namespace proto {

constexpr uint8_t HEAD0 = 0x55;
constexpr uint8_t HEAD1 = 0xAA;
constexpr uint8_t TAIL0 = 0xDD;
constexpr uint8_t TAIL1 = 0xEE;

constexpr uint8_t ID_CO2 = 0x01;
constexpr uint8_t ID_TEMP = 0x02;
constexpr uint8_t ID_HUM = 0x03;
constexpr uint8_t ID_RPM = 0x04;

constexpr uint8_t CAT_EVENT = 0x01;
constexpr uint8_t CAT_PERIOD_EVENT = 0x03;

inline uint16_t crc16Modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

inline QByteArray buildFrame(uint8_t infoId, uint8_t infoCat, const QByteArray &payload)
{
    QByteArray body;
    body.append(char(infoId));
    body.append(char(infoCat));
    body.append(char(payload.size()));
    body.append(payload);

    const uint16_t crc = crc16Modbus(reinterpret_cast<const uint8_t *>(body.constData()),
                                     static_cast<size_t>(body.size()));

    QByteArray frame;
    frame.append(char(HEAD0));
    frame.append(char(HEAD1));
    frame.append(body);
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));
    frame.append(char(TAIL0));
    frame.append(char(TAIL1));
    return frame;
}

inline QByteArray packSet(bool en, float setValue)
{
    QByteArray p(8, 0);
    p[0] = en ? 1 : 0;
    std::memcpy(p.data() + 4, &setValue, sizeof(float));
    return p;
}

inline QByteArray packReal(float realValue)
{
    QByteArray p(4, 0);
    std::memcpy(p.data(), &realValue, sizeof(float));
    return p;
}

struct ParsedFrame {
    uint8_t infoId = 0;
    uint8_t infoCat = 0;
    QByteArray payload;
    bool valid = false;
};

class FrameParser {
public:
    void reset() { state_ = State::WaitHead0; payload_.clear(); len_ = 0; crcRx_ = 0; }

    void feed(const QByteArray &chunk, QList<ParsedFrame> &out)
    {
        for (char b : chunk) {
            feedByte(static_cast<uint8_t>(b), out);
        }
    }

private:
    enum class State {
        WaitHead0,
        WaitHead1,
        Id,
        Cat,
        Len,
        Payload,
        CrcLo,
        CrcHi,
        Tail0,
        Tail1,
    } state_ = State::WaitHead0;

    uint8_t id_ = 0;
    uint8_t cat_ = 0;
    uint8_t len_ = 0;
    QByteArray payload_;
    uint16_t crcRx_ = 0;

    void feedByte(uint8_t b, QList<ParsedFrame> &out)
    {
        switch (state_) {
        case State::WaitHead0:
            if (b == HEAD0) {
                state_ = State::WaitHead1;
            }
            break;
        case State::WaitHead1:
            if (b == HEAD1) {
                state_ = State::Id;
            } else if (b != HEAD0) {
                reset();
            }
            break;
        case State::Id:
            id_ = b;
            state_ = State::Cat;
            break;
        case State::Cat:
            cat_ = b;
            state_ = State::Len;
            break;
        case State::Len:
            len_ = b;
            payload_.clear();
            if (len_ > 64) {
                reset();
                break;
            }
            state_ = len_ ? State::Payload : State::CrcLo;
            break;
        case State::Payload:
            payload_.append(char(b));
            if (payload_.size() >= len_) {
                state_ = State::CrcLo;
            }
            break;
        case State::CrcLo:
            crcRx_ = b;
            state_ = State::CrcHi;
            break;
        case State::CrcHi:
            crcRx_ |= uint16_t(b) << 8;
            state_ = State::Tail0;
            break;
        case State::Tail0:
            if (b == TAIL0) {
                state_ = State::Tail1;
            } else {
                reset();
            }
            break;
        case State::Tail1:
            if (b == TAIL1) {
                QByteArray body;
                body.append(char(id_));
                body.append(char(cat_));
                body.append(char(len_));
                body.append(payload_);
                const uint16_t calc = crc16Modbus(reinterpret_cast<const uint8_t *>(body.constData()),
                                                  static_cast<size_t>(body.size()));
                if (calc == crcRx_) {
                    ParsedFrame f;
                    f.infoId = id_;
                    f.infoCat = cat_;
                    f.payload = payload_;
                    f.valid = true;
                    out.append(f);
                }
            }
            reset();
            break;
        }
    }
};

} // namespace proto
