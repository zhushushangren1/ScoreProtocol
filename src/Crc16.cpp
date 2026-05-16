#include "Crc16.h"

namespace ScoreProtocol {

// 按 CRC-16/CCITT-FALSE 算法逐字节计算校验值。
// data：待校验数据的起始指针。
// length：data 的字节数。
// 返回：16 位 CRC 校验值。
uint16_t crc16Ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }

    return crc;
}

// String 重载：转成字节指针后转调上面的实现。
// text：待校验字符串。
// 返回：16 位 CRC 校验值。
uint16_t crc16Ccitt(const String& text) {
    return crc16Ccitt(reinterpret_cast<const uint8_t*>(text.c_str()), text.length());
}

// 把 16 位 CRC 格式化成 4 位大写十六进制字符串。
// value：CRC 值。
// 返回：固定 4 字符长度的字符串，例如 "0A3F"。
String crc16Hex(uint16_t value) {
    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%04X", value);
    return String(buffer);
}

// 把 4 位十六进制 CRC 文本解析回 16 位整数，接受大小写。
// text：必须正好 4 个字符。
// value：解析成功时被赋值。
// 返回：true=合法；false=长度不对或出现非十六进制字符。
bool parseCrc16Hex(const String& text, uint16_t& value) {
    if (text.length() != 4) {
        return false;
    }

    uint16_t parsed = 0;
    for (uint8_t i = 0; i < 4; i++) {
        const char c = text[i];
        uint8_t nibble = 0;
        if (c >= '0' && c <= '9') {
            nibble = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            nibble = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            nibble = c - 'a' + 10;
        } else {
            return false;
        }

        parsed = static_cast<uint16_t>((parsed << 4) | nibble);
    }

    value = parsed;
    return true;
}

}  // namespace ScoreProtocol
