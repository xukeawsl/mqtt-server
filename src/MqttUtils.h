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

inline bool is_valid_utf8(unsigned char *s, size_t length) {
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

inline bool check_topic_match(const std::string& pub_topic,
                              const std::string& sub_topic) {
    uint32_t pub_len = pub_topic.length();
    uint32_t sub_len = sub_topic.length();
    uint32_t i = 0, j = 0;

    if (pub_len == 0 || sub_len == 0) {
        return true;
    }

    while (i < pub_len && j < sub_len) {
        if (pub_topic[i] == sub_topic[j]) {
            i++;
            j++;
            continue;
        }

        if (sub_topic[j] == '+') {
            if (pub_topic[i] == '/') {
                if (j + 1 < sub_len) {
                    i++;
                    j += 2;    // 后一位一定是 '/', 需要跨过
                } else {
                    return false;
                }
            } else {
                // 匹配一个层级
                while (i < pub_len && pub_topic[i] != '/') {
                    i++;
                }

                if (i == pub_len) {
                    if (j == sub_len - 1) {
                        // 最后一个层级匹配
                        return true;
                    }

                    // example:
                    // pub("xx") sub("+/#")
                    if (j + 2 < sub_len && sub_topic[j + 2] == '#') {
                        return true;
                    }

                    return false;
                }

                // 本层匹配完成, 进入下一层
                if (j + 1 < sub_len) {
                    i++;
                    j += 2;
                }
            }
        } else if (sub_topic[j] == '#') {
            return true;
        } else {
            return false;
        }
    }

    // example:
    // pub("xx/")  sub("xx/#")
    // pub("/") sub("#")
    if (j < sub_len && sub_topic[j] == '#') {
        return true;
    }

    // example:
    // pub("xx") sub("xx/#")
    if (j < sub_len && sub_topic[j] == '/') {
        if (j + 1 < sub_len && sub_topic[j + 1] == '#') {
            return true;
        }
    }

    if (i == pub_len && j == sub_len) {
        return true;
    }

    return false;
}

}