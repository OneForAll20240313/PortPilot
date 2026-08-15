#pragma once

#include "common_types.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// ProtocolEngine（帧切分 / 字段提取 / 组帧）
// 对齐 protocol.schema.json（1.0.0）
// - 所有接口均为纯 C++，不依赖 Qt；不做持久化，只做字节解析与编码
// - 解析时先从字节流切出完整帧，再按字段定义提取
// - 编码时按字段值数组组帧（可选追加 CRC）
// ---------------------------------------------------------------------------

enum class FieldType {
    Uint8, Uint16, Uint32,
    Int8, Int16, Int32,
    Float32, Double,
    Bytes, Ascii, Hex, Bitfield, Bool
};

enum class ByteOrder { Little, Big };

struct FieldDef {
    std::string name;            // 字段标识（字段池引用）
    FieldType type{FieldType::Uint8};
    std::size_t length{1};       // bytes/ascii/hex 必填
    std::optional<ByteOrder> byteOrder;  // 多字节数值必填
    std::size_t offset{0};       // 帧内偏移（字节）
    double scale{1.0};           // 缩放系数
    std::string unit;            // 物理单位
};

enum class LengthType { Fixed, Variable };
enum class SchemaType { Generic, Custom };

struct LengthField {
    std::size_t offset{0};
    int width{2};                // 1 / 2 / 4
    ByteOrder byteOrder{ByteOrder::Big};
};

enum class CrcType { None, Crc8, Crc16 };
enum class CrcPosition { Tail, Head };

struct CrcDef {
    CrcType type{CrcType::None};
    CrcPosition position{CrcPosition::Tail};
};

struct FrameDef {
    std::optional<std::size_t> fixedLength;       // lengthType=fixed 时必填
    std::optional<LengthField> lengthField;       // lengthType=variable 时必填
    Bytes startPattern;                           // 如 0x7E（十六进制字节）
    Bytes endPattern;                             // 如 0x0D 0x0A
    CrcDef crc;
};

struct ProtocolSchema {
    std::string id;
    std::string name;
    SchemaType schemaType{SchemaType::Generic};
    LengthType lengthType{LengthType::Fixed};
    FrameDef frameDef;
    std::vector<FieldDef> fields;
};

// 字段值（与 FieldDef 匹配，按 type 存放变体）
using FieldValue = std::variant<
    std::uint64_t,   // 整型存储（含 uint8/16/32 int8/16/32 bool bitfield）
    double,          // float32/double
    Bytes,           // bytes/hex
    std::string      // ascii
>;

struct Frame {
    Bytes raw;
    std::size_t startOffset{0};  // 在原字节流中的起始偏移
};

struct ParsedFrame {
    Frame frame;
    std::unordered_map<std::string, FieldValue> fields;  // key = FieldDef.name
};

class ProtocolEngine {
public:
    virtual ~ProtocolEngine() = default;

    // ---- 帧切分：从连续字节流中找出符合 schema 的完整帧（A-401 基础）----
    // 返回：已切出的帧列表 + 未匹配的剩余字节（供下次增量解析）
    struct ParseResult {
        std::vector<Frame> frames;
        Bytes leftover;
    };
    virtual ParseResult parseByteStream(const ProtocolSchema& schema, const Bytes& stream) = 0;

    // ---- 字段提取：在单帧上按 fields 定义提取（A-402 基础）----
    virtual Result<ParsedFrame> extractFields(const ProtocolSchema& schema, const Frame& frame) = 0;

    // ---- 组帧：按 field values 编码成完整帧字节（A-412 基础）----
    // values 顺序与 schema.fields 顺序一致
    virtual Result<Bytes> encodeFrame(const ProtocolSchema& schema,
                                      const std::vector<FieldValue>& values) = 0;
};

// ---------------------------------------------------------------------------
// BasicProtocolEngine：ProtocolEngine 的内置默认实现
// - 支持 startPattern / 固定长度 / 长度字段 三种定界方式组合
// - 支持简单 CRC（可扩展）
// ---------------------------------------------------------------------------
class BasicProtocolEngine : public ProtocolEngine {
public:
    ParseResult parseByteStream(const ProtocolSchema& schema, const Bytes& stream) override;
    Result<ParsedFrame> extractFields(const ProtocolSchema& schema, const Frame& frame) override;
    Result<Bytes> encodeFrame(const ProtocolSchema& schema,
                              const std::vector<FieldValue>& values) override;

    // ---- 工具：多字节整数编解码 ----
    static std::uint64_t decode_uint(const Bytes& src, std::size_t offset, int width, ByteOrder bo);
    static std::int64_t  decode_int(const Bytes& src, std::size_t offset, int width, ByteOrder bo);
    static void encode_uint(Bytes& dst, std::size_t offset, int width, ByteOrder bo, std::uint64_t v);
    static void encode_int(Bytes& dst, std::size_t offset, int width, ByteOrder bo, std::int64_t v);
    static Bytes hex_to_bytes(const std::string& hex);
    static std::string bytes_to_hex(const Bytes& b, bool upper = false);

    // CRC（简单实现）
    static std::uint8_t  crc8(const Bytes& data, std::uint8_t poly = 0x07, std::uint8_t init = 0x00);
    static std::uint16_t crc16(const Bytes& data, std::uint16_t poly = 0x1021, std::uint16_t init = 0xFFFF);
};

}  // namespace portpilot::core
