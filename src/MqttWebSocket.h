#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "MqttUtils.h"
#include "MqttWsDefine.h"

using namespace wsdefine;

enum ws_header_status {
  error = -1,
  complete = 0,
  incomplete = -2,
};

class MqttWebSocket {
 public:
  /*
  0               1               2               3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-------+-+-------------+-------------------------------+
  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  | |1|2|3|       |K|             |                               |
  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  |     Extended payload length continued, if payload len == 127  |
  + - - - - - - - - - - - - - - - +-------------------------------+
  |                               |Masking-key, if MASK set to 1  |
  +-------------------------------+-------------------------------+
  | Masking-key (continued)       |          Payload Data         |
  +-------------------------------- - - - - - - - - - - - - - - - +
  :                     Payload Data continued ...                :
  + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  |                     Payload Data continued ...                |
  +---------------------------------------------------------------+
  opcode:
  *  %x0 denotes a continuation frame
  *  %x1 denotes a text frame
  *  %x2 denotes a binary frame
  *  %x3-7 are reserved for further non-control frames
  *  %x8 denotes a connection close
  *  %x9 denotes a ping
  *  %xA denotes a pong
  *  %xB-F are reserved for further control frames
  Payload length:  7 bits, 7+16 bits, or 7+64 bits
  Masking-key:  0 or 4 bytes
  */
  ws_header_status parse_header(const char *buf, size_t size,
                                bool is_server = true) {
    const unsigned char *inp = (const unsigned char *)(buf);

    msg_opcode_ = inp[0] & 0x0F;
    msg_fin_ = (inp[0] >> 7) & 0x01;
    unsigned char msg_masked = (inp[1] >> 7) & 0x01;

    int pos = 2;
    int length_field = inp[1] & (~0x80);

    left_header_len_ = 0;
    if (length_field <= 125) {
      len_bytes_ = SHORT_HEADER;
      payload_length_ = length_field;
    }
    else if (length_field == 126)  // msglen is 16bit!
    {
      len_bytes_ = MEDIUM_HEADER;
      payload_length_ = ntohs(*(uint16_t *)&inp[2]);  // (inp[2] << 8) + inp[3];
      pos += 2;
      left_header_len_ =
          is_server ? MEDIUM_HEADER - size : CLIENT_MEDIUM_HEADER - size;
    }
    else if (length_field == 127)  // msglen is 64bit!
    {
      len_bytes_ = LONG_HEADER;
      payload_length_ = (size_t)be64toh(*(uint64_t *)&inp[2]);
      pos += 8;
      left_header_len_ =
          is_server ? LONG_HEADER - size : CLIENT_LONG_HEADER - size;
    }
    else {
      len_bytes_ = INVALID_HEADER;
      return ws_header_status::error;
    }

    if (msg_masked) {
      std::memcpy(mask_key_, inp + pos, 4);
    }

    return left_header_len_ == 0 ? ws_header_status::complete
                                 : ws_header_status::incomplete;
  }

  int len_bytes() const { return len_bytes_; }

  void reset_len_bytes() { len_bytes_ = SHORT_HEADER; }

  ws_frame_type parse_payload(std::span<char> buf) {
    // unmask data:
    if (*(uint32_t *)mask_key_ != 0) {
      for (size_t i = 0; i < payload_length_; i++) {
        buf[i] = buf[i] ^ mask_key_[i % 4];
      }
    }

    if (msg_opcode_ == 0x0)
      return (msg_fin_)
                 ? ws_frame_type::WS_TEXT_FRAME
                 : ws_frame_type::WS_INCOMPLETE_TEXT_FRAME;  // continuation
    // frame ?
    if (msg_opcode_ == 0x1)
      return (msg_fin_) ? ws_frame_type::WS_TEXT_FRAME
                        : ws_frame_type::WS_INCOMPLETE_TEXT_FRAME;
    if (msg_opcode_ == 0x2)
      return (msg_fin_) ? ws_frame_type::WS_BINARY_FRAME
                        : ws_frame_type::WS_INCOMPLETE_BINARY_FRAME;
    if (msg_opcode_ == 0x8)
      return ws_frame_type::WS_CLOSE_FRAME;
    if (msg_opcode_ == 0x9)
      return ws_frame_type::WS_PING_FRAME;
    if (msg_opcode_ == 0xA)
      return ws_frame_type::WS_PONG_FRAME;
    return ws_frame_type::WS_BINARY_FRAME;
  }

  std::string_view encode_ws_header(size_t size, opcode op, bool eof,
                                    bool need_compression = false,
                                    bool is_client = true) {
    frame_header hdr{};
    hdr.fin = eof;
    hdr.rsv1 = 0;
    if (need_compression)
      hdr.rsv2 = 1;
    else
      hdr.rsv2 = 0;
    hdr.rsv3 = 0;
    hdr.opcode = static_cast<uint8_t>(op);
    hdr.mask = is_client;

    hdr.len = size < 126 ? size : (size < 65536 ? 126 : 127);

    std::memcpy(msg_header_, (char *)&hdr, sizeof(hdr));

    size_t len_bytes = 0;
    if (size >= 126) {
      if (size >= 65536) {
        len_bytes = 8;
        *((uint64_t *)(msg_header_ + 2)) = htobe64(size);
      }
      else {
        len_bytes = 2;
        *((uint16_t *)(msg_header_ + 2)) = htons(static_cast<uint16_t>(size));
      }
    }

    size_t header_len = 6;

    if (is_client) {
      if (size > 0) {
        // generate mask key.
        uint32_t random = (uint32_t)rand();
        memcpy(mask_key_, &random, 4);
      }

      std::memcpy(msg_header_ + 2 + len_bytes, mask_key_, 4);
    }
    else {
      header_len = 2;
    }

    return {msg_header_, header_len + len_bytes};
  }

  close_frame parse_close_payload(char *src, size_t length) {
    close_frame cf = {};
    if (length >= 2) {
      std::memcpy(&cf.code, src, 2);
      cf = {ntohs(cf.code), src + 2, length - 2};
      if (cf.code < 1000 || cf.code > 4999 ||
          (cf.code > 1011 && cf.code < 4000) ||
          (cf.code >= 1004 && cf.code <= 1006) ||
          !utils::is_valid_utf8((unsigned char *)cf.message, cf.length)) {
        return {};
      }
    }
    return cf;
  }

  std::string format_close_payload(uint16_t code, char *message,
                                   size_t length) {
    std::string close_payload;
    close_payload.resize(length + 2);
    code = htons(code);
    std::memcpy(close_payload.data(), &code, 2);

    if (length > 0) {
      std::memcpy(close_payload.data() + 2, message, length);
    }
    return close_payload;
  }

  size_t left_header_len() const { return left_header_len_; }

  size_t payload_length() const { return payload_length_; }

private:
  size_t payload_length_ = 0;

  size_t left_header_len_ = 2;
  uint8_t mask_key_[4] = {};
  unsigned char msg_opcode_ = 0;
  unsigned char msg_fin_ = 0;

  char msg_header_[14];
  ws_head_len len_bytes_ = SHORT_HEADER;
};
