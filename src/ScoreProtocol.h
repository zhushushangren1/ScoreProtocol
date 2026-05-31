// 共享文本协议接口。
// 定义服务端和裁判端共同使用的消息类型、解析结果结构和组帧/解析函数。
#ifndef SCORE_PROTOCOL_H
#define SCORE_PROTOCOL_H

#include <Arduino.h>

namespace ScoreProtocol {

// 协议帧字段数量上限（含末尾 CRC 字段），用于 ParsedFrame.fields 数组定长。
constexpr uint8_t MAX_FIELDS = 12;
// 单个字段允许的最大字符数，超过则视为非法帧丢弃。
constexpr uint8_t MAX_FIELD_LENGTH = 24;

// 协议消息类型枚举，对应文档第 6 节里 CSV 帧的第一个字段。
enum class MessageType : uint8_t {
    Unknown,
    Hello,
    Heartbeat,
    Submit,
    Ack,
    Status,
    Assign,
    AssignAck,
    Unbind,
    UnbindAck
};

// 解析后的协议帧。
// type：识别出的消息类型；Unknown 表示首字段无法匹配任何已知类型。
// fieldCount：解析出的字段数（不含末尾 CRC，因为 CRC 在校验后已被去除）。
// fields：各字段文本，下标 0 是消息类型，后续按对应消息的格式排列。
struct ParsedFrame {
    MessageType type = MessageType::Unknown;
    uint8_t fieldCount = 0;
    String fields[MAX_FIELDS];
};

// 把文本（"HELLO"、"SUBMIT" 等）转换成 MessageType 枚举。
// value：消息类型字符串。
// 返回：对应枚举；不识别时返回 MessageType::Unknown。
MessageType messageTypeFromString(const String& value);

// 把 MessageType 转回字符串字面量，供日志/组帧使用。
// type：枚举值。
// 返回：常量字符串指针；未知类型返回 "UNKNOWN"。
const char* messageTypeToString(MessageType type);

// 把若干字段拼成"字段1,字段2,...,字段N,CRC\n"的协议行。
// fields：字段文本数组，下标 0 必须是消息类型字符串。
// fieldCount：fields 中有效元素个数，必须 > 0 且 < MAX_FIELDS（要给 CRC 留位置）。
// 返回：可直接通过 LoRa UART 发送的完整一行；参数非法时返回空字符串。
String buildFrame(const String fields[], uint8_t fieldCount);

// 校验并解析一条协议行。
// line：从 LoRa UART 读到的一行（可带前后空白和 \r\n，函数内部会 trim）。
// frame：解析成功时被填充；失败时其状态未定义但 type 会保持 Unknown。
// 返回：true=CRC 通过且首字段是已知消息类型；false=非法/损坏/未知类型。
bool parseFrame(const String& line, ParsedFrame& frame);

// 把纯数字文本解析成 unsigned long，遇到非数字、空串或溢出都失败。
// text：待解析的十进制数字串。
// value：解析成功时被赋值。
// 返回：true=成功；false=格式不合法或溢出。
bool parseUnsignedLong(const String& text, unsigned long& value);

// 把数字文本解析成 int，并校验范围 [minValue, maxValue]。
// text：待解析的十进制数字串（不接受负号）。
// minValue/maxValue：允许的闭区间。
// value：解析成功时被赋值。
// 返回：true=合法且在范围内；false=格式错误或越界。
bool parseIntInRange(const String& text, int minValue, int maxValue, int& value);

}  // namespace ScoreProtocol

#endif  // SCORE_PROTOCOL_H
