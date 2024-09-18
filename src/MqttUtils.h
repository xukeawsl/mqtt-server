#pragma once

#include <cctype>
#include <string>

namespace utils {

inline const char *MAP =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

inline const char *MAP_URL_ENCODED =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";
inline size_t base64_encode(char *_dst, const void *_src, size_t len,
                            int url_encoded) {
  char *dst = _dst;
  const uint8_t *src = reinterpret_cast<const uint8_t *>(_src);
  const char *map = url_encoded ? MAP_URL_ENCODED : MAP;
  uint32_t quad;

  for (; len >= 3; src += 3, len -= 3) {
    quad = ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
    *dst++ = map[quad >> 18];
    *dst++ = map[(quad >> 12) & 63];
    *dst++ = map[(quad >> 6) & 63];
    *dst++ = map[quad & 63];
  }
  if (len != 0) {
    quad = (uint32_t)src[0] << 16;
    *dst++ = map[quad >> 18];
    if (len == 2) {
      quad |= (uint32_t)src[1] << 8;
      *dst++ = map[(quad >> 12) & 63];
      *dst++ = map[(quad >> 6) & 63];
      if (!url_encoded)
        *dst++ = '=';
    }
    else {
      *dst++ = map[(quad >> 12) & 63];
      if (!url_encoded) {
        *dst++ = '=';
        *dst++ = '=';
      }
    }
  }

  *dst = '\0';
  return dst - _dst;
}

bool is_valid_utf8(unsigned char *s, size_t length) {
  for (unsigned char *e = s + length; s != e;) {
    if (s + 4 <= e && ((*(uint32_t *)s) & 0x80808080) == 0) {
      s += 4;
    }
    else {
      while (!(*s & 0x80)) {
        if (++s == e) {
          return true;
        }
      }

      if ((s[0] & 0x60) == 0x40) {
        if (s + 1 >= e || (s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) {
          return false;
        }
        s += 2;
      }
      else if ((s[0] & 0xf0) == 0xe0) {
        if (s + 2 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
            (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||
            (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
          return false;
        }
        s += 3;
      }
      else if ((s[0] & 0xf8) == 0xf0) {
        if (s + 3 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
            (s[3] & 0xc0) != 0x80 || (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||
            (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) {
          return false;
        }
        s += 4;
      }
      else {
        return false;
      }
    }
  }
  return true;
}

}