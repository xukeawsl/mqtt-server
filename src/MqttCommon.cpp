#include "MqttCommon.h"

namespace util {

bool check_topic_match(const std::string& pub_topic,
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

}    // namespace util