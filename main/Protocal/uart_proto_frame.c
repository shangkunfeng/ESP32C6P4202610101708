#include "uart_proto_frame.h"

#include "protocol_config.h"
#include "uart_proto_crc.h"

#include <string.h>

typedef enum {
    PARSE_WAIT_HEAD0 = 0,
    PARSE_WAIT_HEAD1,
    PARSE_INFO_ID,
    PARSE_INFO_CAT,
    PARSE_INFO_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI,
    PARSE_TAIL0,
    PARSE_TAIL1,
} uart_proto_parse_state_t;

static struct {
    uart_proto_parse_state_t state;
    uart_proto_frame_t frame;
    size_t payload_idx;
    uint16_t crc_rx;
} s_parse;

static void uart_proto_parse_reset(void)
{
    s_parse.state = PARSE_WAIT_HEAD0;
    s_parse.payload_idx = 0;
    s_parse.crc_rx = 0;
    memset(&s_parse.frame, 0, sizeof(s_parse.frame));
}

static bool uart_proto_parse_finish(uart_proto_frame_t *out)
{
    const size_t crc_len = 1 + 1 + 1 + s_parse.frame.payload_len;
    uint8_t crc_buf[3 + UART_PROTO_FRAME_MAX_PAYLOAD];
<<<<<<< HEAD
<<<<<<< HEAD
    
=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    if (crc_len > sizeof(crc_buf)) {
        uart_proto_parse_reset();
        return false;
    }

    crc_buf[0] = s_parse.frame.info_id;
    crc_buf[1] = s_parse.frame.info_cat;
    crc_buf[2] = (uint8_t)s_parse.frame.payload_len;
    if (s_parse.frame.payload_len > 0) {
        memcpy(&crc_buf[3], s_parse.frame.payload, s_parse.frame.payload_len);
    }

    uint16_t calc = uart_proto_crc16(crc_buf, crc_len);
    if (calc != s_parse.crc_rx) {
        uart_proto_parse_reset();
        return false;
    }

    if (out) {
        *out = s_parse.frame;
    }
    uart_proto_parse_reset();
    return true;
}

static void uart_proto_parse_byte(uint8_t b, uart_proto_frame_t *out, bool *frame_ready)
{
    switch (s_parse.state) {
    case PARSE_WAIT_HEAD0:
        if (b == PROTO_FRAME_HEAD_0) {
            s_parse.state = PARSE_WAIT_HEAD1;
        }
        break;
    case PARSE_WAIT_HEAD1:
        if (b == PROTO_FRAME_HEAD_1) {
            s_parse.state = PARSE_INFO_ID;
        } else if (b == PROTO_FRAME_HEAD_0) {
            s_parse.state = PARSE_WAIT_HEAD1;
        } else {
            uart_proto_parse_reset();
        }
        break;
    case PARSE_INFO_ID:
        s_parse.frame.info_id = b;
        s_parse.state = PARSE_INFO_CAT;
        break;
    case PARSE_INFO_CAT:
        s_parse.frame.info_cat = b;
        s_parse.state = PARSE_INFO_LEN;
        break;
    case PARSE_INFO_LEN:
        s_parse.frame.payload_len = b;
        s_parse.payload_idx = 0;
        if (s_parse.frame.payload_len > UART_PROTO_FRAME_MAX_PAYLOAD) {
            uart_proto_parse_reset();
            break;
        }
        if (s_parse.frame.payload_len == 0) {
            s_parse.state = PARSE_CRC_LO;
        } else {
            s_parse.state = PARSE_PAYLOAD;
        }
        break;
    case PARSE_PAYLOAD:
        s_parse.frame.payload[s_parse.payload_idx++] = b;
        if (s_parse.payload_idx >= s_parse.frame.payload_len) {
            s_parse.state = PARSE_CRC_LO;
        }
        break;
    case PARSE_CRC_LO:
        s_parse.crc_rx = b;
        s_parse.state = PARSE_CRC_HI;
        break;
    case PARSE_CRC_HI:
        s_parse.crc_rx |= (uint16_t)b << 8;
        s_parse.state = PARSE_TAIL0;
        break;
    case PARSE_TAIL0:
        if (b == PROTO_FRAME_TAIL_0) {
            s_parse.state = PARSE_TAIL1;
        } else {
            uart_proto_parse_reset();
        }
        break;
    case PARSE_TAIL1:
        if (b == PROTO_FRAME_TAIL_1) {
            bool ok = uart_proto_parse_finish(out);
            if (ok && frame_ready) {
                *frame_ready = true;
            }
        } else {
            uart_proto_parse_reset();
        }
        break;
    default:
        uart_proto_parse_reset();
        break;
    }
}

esp_err_t uart_proto_frame_feed(const uint8_t *chunk, size_t chunk_len, uart_proto_frame_t *out, bool *frame_ready)
{
<<<<<<< HEAD
<<<<<<< HEAD
    if (!chunk || chunk_len == 0) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_ready) 
    {
        *frame_ready = false;
    }

    for (size_t i = 0; i < chunk_len; i++) 
    {
        bool ready = false;
        uart_proto_parse_byte(chunk[i], out, &ready);

        if (ready && frame_ready) 
        {
=======
=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    if (!chunk || chunk_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame_ready) {
        *frame_ready = false;
    }

    for (size_t i = 0; i < chunk_len; i++) {
        bool ready = false;
        uart_proto_parse_byte(chunk[i], out, &ready);
        if (ready && frame_ready) {
<<<<<<< HEAD
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
            *frame_ready = true;
            return ESP_OK;
        }
    }
    return ESP_OK;
}

esp_err_t uart_proto_frame_build(const uart_proto_frame_t *frame, uint8_t *buf, size_t buf_size, size_t *written)
{
    if (!frame || !buf || !written) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->payload_len > 255) {
        return ESP_ERR_INVALID_SIZE;
    }

    const size_t need = 2 + 1 + 1 + 1 + frame->payload_len + 2 + 2;
    if (buf_size < need) {
        return ESP_ERR_NO_MEM;
    }

    size_t i = 0;
    buf[i++] = PROTO_FRAME_HEAD_0;
    buf[i++] = PROTO_FRAME_HEAD_1;
    buf[i++] = frame->info_id;
    buf[i++] = frame->info_cat;
    buf[i++] = (uint8_t)frame->payload_len;
    if (frame->payload_len > 0) {
        memcpy(&buf[i], frame->payload, frame->payload_len);
        i += frame->payload_len;
    }

    const size_t crc_len = 1 + 1 + 1 + frame->payload_len;
    uint16_t crc = uart_proto_crc16(&buf[2], crc_len);
    buf[i++] = (uint8_t)(crc & 0xFF);
    buf[i++] = (uint8_t)((crc >> 8) & 0xFF);
    buf[i++] = PROTO_FRAME_TAIL_0;
    buf[i++] = PROTO_FRAME_TAIL_1;

    *written = i;
    return ESP_OK;
}

void uart_proto_frame_parser_reset(void)
{
    uart_proto_parse_reset();
}
