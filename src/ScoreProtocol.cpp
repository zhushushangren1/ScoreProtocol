#include "ScoreProtocol.h"

#include "Crc16.h"

namespace ScoreProtocol {

// 把消息类型字符串映射成 MessageType 枚举。
// value：协议行第一个 CSV 字段，例如 "HELLO"、"SUBMIT"。
// 返回：对应枚举；不识别返回 MessageType::Unknown。
MessageType messageTypeFromString(const String& value) {
    if (value == "HELLO") return MessageType::Hello;
    if (value == "HEARTBEAT") return MessageType::Heartbeat;
    if (value == "SUBMIT") return MessageType::Submit;
    if (value == "ACK") return MessageType::Ack;
    if (value == "STATUS") return MessageType::Status;
    if (value == "ASSIGN") return MessageType::Assign;
    if (value == "ASSIGN_ACK") return MessageType::AssignAck;
    if (value == "UNBIND") return MessageType::Unbind;
    if (value == "UNBIND_ACK") return MessageType::UnbindAck;
    return MessageType::Unknown;
}

// 把 MessageType 枚举转回字符串字面量，供组帧和日志使用。
// type：枚举值。
// 返回：常量字符串指针；未知类型返回 "UNKNOWN"。
const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::Hello:
            return "HELLO";
        case MessageType::Heartbeat:
            return "HEARTBEAT";
        case MessageType::Submit:
            return "SUBMIT";
        case MessageType::Ack:
            return "ACK";
        case MessageType::Status:
            return "STATUS";
        case MessageType::Assign:
            return "ASSIGN";
        case MessageType::AssignAck:
            return "ASSIGN_ACK";
        case MessageType::Unbind:
            return "UNBIND";
        case MessageType::UnbindAck:
            return "UNBIND_ACK";
        case MessageType::Unknown:
        default:
            return "UNKNOWN";
    }
}

// 把若干字段拼成"字段1,字段2,...,字段N,CRC\n"的完整协议行。
// fields：字段文本数组，fields[0] 必须是消息类型字符串。
// fieldCount：fields 中有效元素个数；必须 > 0 且 < MAX_FIELDS（要给 CRC 留位置）。
// 返回：可直接通过 LoRa UART 发送的整行；参数非法时返回空字符串。
String buildFrame(const String fields[], uint8_t fieldCount) {
    if (fieldCount == 0 || fieldCount >= MAX_FIELDS) {
        // fieldCount 为 0 没有消息类型；>= MAX_FIELDS 会挤掉末尾 CRC 字段空间。
        return String();
    }

    String payload;
    for (uint8_t i = 0; i < fieldCount; i++) {
        if (i > 0) {
            // CRC 只覆盖逗号分隔的 payload，不包含最后的 ",CRC\n"。
            payload += ",";
        }
        payload += fields[i];
    }

    // 发送格式固定为 payload + "," + 4位CRC + "\n"，LoRaLink 依赖换行判断帧结束。
    return payload + "," + crc16Hex(crc16Ccitt(payload)) + "\n";
}

// 解析一条协议行，校验 CRC、拆分字段、识别消息类型。
// line：从 LoRa UART 读到的一行，可带前后空白和 \r\n。
// frame：解析成功时被填充（type、fieldCount、fields）。
// 返回：true=CRC 通过且首字段是已知消息类型；false=非法、损坏、字段超长或类型未知。
bool parseFrame(const String& line, ParsedFrame& frame) {
    // 先清空输出结构，避免解析失败时调用方误读上一帧残留字段。
    frame = ParsedFrame();

    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) {
        // 空行不算协议帧，常见于 CRLF 或串口噪声。
        return false;
    }

    // 找最后一个逗号，把行拆成 "payload" 和 "CRC 文本" 两部分。
    int lastComma = trimmed.lastIndexOf(',');
    if (lastComma <= 0 || lastComma == static_cast<int>(trimmed.length()) - 1) {
        // 没有逗号、payload 为空、CRC 为空都不是合法帧。
        return false;
    }

    const String payload = trimmed.substring(0, lastComma);
    const String crcText = trimmed.substring(lastComma + 1);

    uint16_t expected = 0;
    if (!parseCrc16Hex(crcText, expected)) {
        // CRC 字段必须正好 4 个十六进制字符。
        return false;
    }

    const uint16_t actual = crc16Ccitt(payload);
    if (actual != expected) {
        // CRC 不一致说明空中数据或串口数据损坏，业务层不能继续处理。
        return false;
    }

    // CRC 通过后，再按逗号切分 payload 写入 frame.fields。
    uint8_t count = 0;
    int start = 0;
    while (start <= static_cast<int>(payload.length())) {
        if (count >= MAX_FIELDS) {
            // 字段数超过定长数组容量时拒绝，避免写越界。
            return false;
        }

        int comma = payload.indexOf(',', start);
        if (comma < 0) {
            // 没找到下一个逗号，说明当前字段是最后一个字段。
            comma = payload.length();
        }

        String field = payload.substring(start, comma);
        if (field.length() > MAX_FIELD_LENGTH) {
            // 限制单字段长度，防止异常长字符串占用过多 ESP32 堆内存。
            return false;
        }

        frame.fields[count++] = field;
        start = comma + 1;

        if (comma == static_cast<int>(payload.length())) {
            // 已切到 payload 末尾，退出循环。
            break;
        }
    }

    if (count == 0) {
        // 理论上 lastComma 校验后不会出现，保留防御逻辑。
        return false;
    }

    frame.fieldCount = count;
    frame.type = messageTypeFromString(frame.fields[0]);
    // 未知消息类型直接返回 false，上层会打印 Invalid protocol frame。
    return frame.type != MessageType::Unknown;
}

// 把纯数字文本解析成 unsigned long。
// text：十进制数字串；空串或含非数字字符都判失败。
// value：解析成功时被赋值。
// 返回：true=合法；false=格式不合法或乘 10 累加时溢出。
bool parseUnsignedLong(const String& text, unsigned long& value) {
    if (text.length() == 0) {
        // 空串不按 0 处理，避免缺失字段被误接受。
        return false;
    }

    unsigned long parsed = 0;
    for (uint16_t i = 0; i < text.length(); i++) {
        const char c = text[i];
        if (c < '0' || c > '9') {
            // 不接受符号、小数点、空格，协议数字字段必须是纯十进制。
            return false;
        }

        const unsigned long next = parsed * 10UL + static_cast<unsigned long>(c - '0');
        // 溢出检测：累加后比之前还小说明翻转了。
        if (next < parsed) {
            return false;
        }
        parsed = next;
    }

    value = parsed;
    // 只有完整扫描通过后才写 value，调用方可认为 value 是可信数字。
    return true;
}

// 把数字文本解析成 int 并做范围校验。
// text：十进制数字串（不接受负号）。
// minValue/maxValue：允许的闭区间。
// value：解析成功时被赋值。
// 返回：true=合法且在范围内；false=格式错误、越界或超过 int 上限。
bool parseIntInRange(const String& text, int minValue, int maxValue, int& value) {
    unsigned long parsed = 0;
    if (!parseUnsignedLong(text, parsed)) {
        // 先复用无符号解析，统一拒绝空串、负号和非数字字符。
        return false;
    }

    if (parsed > static_cast<unsigned long>(maxValue)) {
        // 先和 maxValue 比较，可避免后面转 int 时溢出。
        return false;
    }

    const int parsedInt = static_cast<int>(parsed);
    if (parsedInt < minValue || parsedInt > maxValue) {
        // minValue 当前主要用于 0，但保留通用闭区间校验。
        return false;
    }

    value = parsedInt;
    return true;
}

}  // namespace ScoreProtocol
