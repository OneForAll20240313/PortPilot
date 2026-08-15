// Plugin 扩展层数据结构定义
// 对齐契约：protocol.schema.json、command-group.schema.json、macro.schema.json
// 架构定位：DDD 五层架构之 Plugin 横向扩展层（13.1），挂载到 Service 层
#ifndef PORTPILOT_PLUGIN_PLUGIN_TYPES_H
#define PORTPILOT_PLUGIN_PLUGIN_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace portpilot::plugin {

// ===== 协议模板数据结构（对齐 protocol.schema.json）=====

/// 字段类型（对齐 protocol.schema.json fields[].type）
enum class FieldType {
    UInt8, UInt16, UInt32,
    Int8, Int16, Int32,
    Float32, Double,
    Bytes, Ascii, Hex,
    Bitfield, Bool
};

/// 字节序（对齐 protocol.schema.json byteOrder）
enum class ByteOrder { Little, Big };

/// 协议字段定义（对齐 protocol.schema.json fields[]）
struct ProtocolField {
    std::string name;                       ///< 字段标识，用于字段池引用
    FieldType type = FieldType::UInt8;      ///< 字段类型
    int length = 0;                         ///< 字段长度（字节），bytes/ascii/hex 必填
    ByteOrder byteOrder = ByteOrder::Little; ///< 字节序，多字节数值类型必填
    int offset = 0;                         ///< 字段在帧内偏移（字节）
    double scale = 1.0;                     ///< 缩放系数
    std::string unit;                       ///< 物理单位，如 mV、°C
};

/// CRC 校验配置（对齐 protocol.schema.json frameDef.crc）
struct CrcConfig {
    enum class Type { Crc8, Crc16, None };
    enum class Position { Tail, Head };
    Type type = Type::None;
    Position position = Position::Tail;
};

/// 长度字段定位（对齐 protocol.schema.json frameDef.lengthField，variable 长度用）
struct LengthField {
    int offset = 0;                         ///< 长度字段在帧内偏移
    int width = 1;                          ///< 长度字段宽度（1/2/4 字节）
    ByteOrder byteOrder = ByteOrder::Little;
};

/// 帧定义（对齐 protocol.schema.json frameDef）
struct FrameDef {
    int fixedLength = 0;                    ///< lengthType=fixed 时的固定帧长（字节）
    LengthField lengthField;                ///< lengthType=variable 时的长度字段定位
    std::string startPattern;               ///< 帧起始标识（十六进制字节序列，如 7E）
    std::string endPattern;                 ///< 帧结束标识（十六进制，如 0D0A）
    CrcConfig crc;                          ///< 帧校验配置（可选）
};

/// 协议模式（对齐 protocol.schema.json schemaType）
enum class SchemaType { Generic, Custom };

/// 帧长度类型（对齐 protocol.schema.json lengthType）
enum class LengthType { Fixed, Variable };

/// 协议模板（对齐 protocol.schema.json 顶层）
struct ProtocolTemplate {
    std::string id;                         ///< 协议唯一标识
    std::string name;                       ///< 协议名称
    SchemaType schemaType = SchemaType::Generic;
    LengthType lengthType = LengthType::Fixed;
    FrameDef frameDef;                      ///< 帧定义
    std::vector<ProtocolField> fields;      ///< 字段定义列表
};

/// 模板管理操作（对齐 A-413 TemplateAction，action ∈ {add,update,delete,list}）
enum class TemplateAction { Add, Update, Delete, List };

// ===== 自定义命令数据结构（对齐 command-group.schema.json / macro.schema.json）=====

/// 命令类型（对齐 command-group.schema.json commands[].type）
enum class CommandType {
    Send, Wait, Loop, Condition, Sleep, Read
};

/// 命令项（对齐 command-group.schema.json commands[]，支持递归嵌套）
struct CommandItem {
    std::string id;                         ///< 命令唯一标识
    CommandType type = CommandType::Send;   ///< 命令类型
    std::string data;                       ///< send/read 的载荷（协议字段名或原始字节十六进制）
    int delayMs = 0;                        ///< 命令执行前延时（毫秒）
    int loopCount = 1;                      ///< loop 类型的循环次数
    std::vector<CommandItem> children;      ///< 子命令（loop/condition 嵌套）
};

/// 命令组状态（对齐 command-group.schema.json state 五态）
enum class CommandState { Pending, Running, Paused, Done, Error };

/// 脚本数据（对齐 command-group.schema.json 顶层）
struct ScriptData {
    std::string id;                         ///< 命令组唯一标识（UUID）
    std::string sessionId;                  ///< 所属会话 ID
    std::string name;                       ///< 命令组显示名称
    std::string description;                ///< 命令组说明（可选）
    std::vector<CommandItem> commands;      ///< 命令序列
    bool enabled = true;                    ///< 是否启用
    CommandState state = CommandState::Pending;
    int64_t createdAt = 0;                  ///< 创建时间戳（毫秒）
};

/// 脚本管理操作（对齐 A-715 ScriptAction，action ∈ {import,edit,delete,list}）
enum class ScriptAction { Import, Edit, Delete, List };

} // namespace portpilot::plugin

#endif // PORTPILOT_PLUGIN_PLUGIN_TYPES_H
