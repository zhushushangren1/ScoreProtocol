#ifndef SCORE_PROTOCOL_CRC16_H
#define SCORE_PROTOCOL_CRC16_H

#include <Arduino.h>

namespace ScoreProtocol {

// 计算 CRC-16/CCITT-FALSE 校验值（多项式 0x1021，初始值 0xFFFF，不反转，不异或输出）。
// 用于裁判机/服务端两端对协议帧 payload 做一致性校验。
// data：待校验数据的起始指针。
// length：data 的字节数。
// 返回：16 位 CRC 校验值。
uint16_t crc16Ccitt(const uint8_t* data, size_t length);

// 上一函数的 String 重载，便于直接对协议字符串 payload 计算 CRC。
// text：待校验字符串（不包含末尾 CRC 字段本身）。
// 返回：16 位 CRC 校验值。
uint16_t crc16Ccitt(const String& text);

// 把 16 位 CRC 转成 4 位大写十六进制字符串，作为协议帧最后一个字段。
// value：crc16Ccitt 算出的 CRC 值。
// 返回：固定 4 字符的大写十六进制字符串，例如 "0A3F"。
String crc16Hex(uint16_t value);

// 把 4 位十六进制 CRC 文本解析回 16 位整数；接受大小写。
// text：待解析的 CRC 字符串，必须正好 4 个字符。
// value：解析成功时写入此引用。
// 返回：true=解析成功；false=长度不对或出现非十六进制字符。
bool parseCrc16Hex(const String& text, uint16_t& value);

}  // namespace ScoreProtocol

#endif  // SCORE_PROTOCOL_CRC16_H
