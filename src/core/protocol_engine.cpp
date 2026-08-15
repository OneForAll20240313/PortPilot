#include "protocol_engine.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace portpilot::core {

// ---------------------------------------------------------------------------
// BasicProtocolEngine 工具函数
// ---------------------------------------------------------------------------

std::uint64_t BasicProtocolEngine::decode_uint(const Bytes& src, std::size_t offset, int width, ByteOrder bo) {
    if (offset + width > src.size()) return 0;
    std::uint64_t v = 0;
    if (bo == ByteOrder::Big) {
        for (int i = 0; i < width; ++i) v = (v << 8) | src[offset + i];
    } else {
        for (int i = width - 1; i >= 0; --i) v = (v << 8) | src[offset + i];
    }
    return v;
}

std::int64_t BasicProtocolEngine::decode_int(const Bytes& src, std::size_t offset, int width, ByteOrder bo) {
    const auto u = decode_uint(src, offset, width, bo);
    // 符号扩展
    const int bits = width * 8;
    std::uint64_t sign = std::uint64_t{1} << (bits - 1);
    if (u & sign) {
        std::uint64_t mask = (bits == 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bits) - 1);
        return static_cast<std::int64_t>(u | ~mask);
    }
    return static_cast<std::int64_t>(u);
}

void BasicProtocolEngine::encode_uint(Bytes& dst, std::size_t offset, int width, ByteOrder bo, std::uint64_t v) {
    if (offset + width > dst.size()) dst.resize(offset + width);
    if (bo == ByteOrder::Big) {
        for (int i = width - 1; i >= 0; --i) {
            dst[offset + i] = static_cast<std::uint8_t>(v & 0xFF);
            v >>= 8;
        }
    } else {
        for (int i = 0; i < width; ++i) {
            dst[offset + i] = static_cast<std::uint8_t>(v & 0xFF);
            v >>= 8;
        }
    }
}

void BasicProtocolEngine::encode_int(Bytes& dst, std::size_t offset, int width, ByteOrder bo, std::int64_t v) {
    encode_uint(dst, offset, width, bo, static_cast<std::uint64_t>(v));
}

Bytes BasicProtocolEngine::hex_to_bytes(const std::string& hex) {
    Bytes out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned int byte = 0;
        std::istringstream ss(hex.substr(i, 2));
        ss >> std::hex >> byte;
        out.push_back(static_cast<std::uint8_t>(byte));
    }
    return out;
}

std::string BasicProtocolEngine::bytes_to_hex(const Bytes& b, bool upper) {
    std::ostringstream ss;
    if (upper) ss << std::uppercase;
    ss << std::hex << std::setfill('0');
    for (auto v : b) ss << std::setw(2) << static_cast<unsigned int>(v);
    return ss.str();
}

std::uint8_t BasicProtocolEngine::crc8(const Bytes& data, std::uint8_t poly, std::uint8_t init) {
    std::uint8_t crc = init;
    for (auto b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80) crc = static_cast<std::uint8_t>((crc << 1) ^ poly);
            else crc <<= 1;
        }
    }
    return crc;
}

std::uint16_t BasicProtocolEngine::crc16(const Bytes& data, std::uint16_t poly, std::uint16_t init) {
    std::uint16_t crc = init;
    for (auto b : data) {
        crc ^= static_cast<std::uint16_t>(b) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) crc = static_cast<std::uint16_t>((crc << 1) ^ poly);
            else crc <<= 1;
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// 帧切分
// ---------------------------------------------------------------------------
BasicProtocolEngine::ParseResult BasicProtocolEngine::parseByteStream(
    const ProtocolSchema& schema, const Bytes& stream) {
    ParseResult out;
    Bytes work = stream;
    std::size_t baseOffset = 0;

    while (!work.empty()) {
        // 1. 匹配起始码
        std::size_t startIdx = 0;
        if (!schema.frameDef.startPattern.empty()) {
            const auto& sp = schema.frameDef.startPattern;
            auto it = std::search(work.begin(), work.end(), sp.begin(), sp.end());
            if (it == work.end()) break;  // 没有起始码，整段留作剩余
            startIdx = static_cast<std::size_t>(it - work.begin());
            if (startIdx != 0) {
                // 起始码前的字节丢弃（不构成完整帧）
                work.erase(work.begin(), it);
                baseOffset += startIdx;
                startIdx = 0;
            }
        }

        // 2. 计算帧长
        std::size_t frameLen = 0;
        if (schema.lengthType == LengthType::Fixed) {
            if (!schema.frameDef.fixedLength) {
                break;  // schema 异常，剩余全部留作
            }
            frameLen = *schema.frameDef.fixedLength;
        } else {  // Variable
            if (!schema.frameDef.lengthField) break;
            const auto& lf = *schema.frameDef.lengthField;
            if (work.size() < lf.offset + static_cast<std::size_t>(lf.width)) {
                break;  // 长度字段还没收齐，等待更多
            }
            const auto len = decode_uint(work, lf.offset, lf.width, lf.byteOrder);
            // 把长度字段之前的 startPattern 等也算进总长度（常见协议实现为 帧头+长度+数据，长度值不包含自身）
            // 这里取保守：总帧长 = 长度字段偏移 + 长度字段宽度 + len
            frameLen = lf.offset + lf.width + static_cast<std::size_t>(len);
            // endPattern（可选）
            if (!schema.frameDef.endPattern.empty()) {
                // 要求 frameLen 后还能看到 endPattern，否则修正
                const auto& ep = schema.frameDef.endPattern;
                if (work.size() < frameLen + ep.size()) break;
                if (!std::equal(ep.begin(), ep.end(), work.begin() + frameLen)) break;
                frameLen += ep.size();
            }
            if (frameLen < lf.offset + lf.width) break;  // 非法帧长
        }

        // 3. CRC（简单校验）
        auto tmpFrameLen = frameLen;
        if (schema.frameDef.crc.type != CrcType::None) {
            const int crcWidth = (schema.frameDef.crc.type == CrcType::Crc8) ? 1 : 2;
            if (schema.frameDef.crc.position == CrcPosition::Tail) {
                tmpFrameLen += crcWidth;
            }
        }

        if (work.size() < tmpFrameLen) break;  // 数据不足，等下次

        // 4. 组装 Frame
        Frame f;
        f.startOffset = baseOffset + startIdx;
        f.raw.assign(work.begin(), work.begin() + tmpFrameLen);
        out.frames.push_back(std::move(f));

        work.erase(work.begin(), work.begin() + tmpFrameLen);
        baseOffset += tmpFrameLen;
    }

    out.leftover = std::move(work);
    return out;
}

// ---------------------------------------------------------------------------
// 字段提取
// ---------------------------------------------------------------------------
Result<ParsedFrame> BasicProtocolEngine::extractFields(const ProtocolSchema& schema, const Frame& frame) {
    ParsedFrame out;
    out.frame = frame;
    const auto& raw = frame.raw;

    for (const auto& fd : schema.fields) {
        if (fd.offset >= raw.size()) {
            return make_err<ParsedFrame>(ErrorCode::PROTO_FIELD_INVALID,
                                          "字段偏移超出帧长: " + fd.name);
        }
        FieldValue v;
        switch (fd.type) {
            case FieldType::Uint8:
            case FieldType::Uint16:
            case FieldType::Uint32: {
                const int w = (fd.type == FieldType::Uint8) ? 1 : (fd.type == FieldType::Uint16 ? 2 : 4);
                const auto bo = fd.byteOrder.value_or(ByteOrder::Big);
                v = decode_uint(raw, fd.offset, w, bo);
                break;
            }
            case FieldType::Int8:
            case FieldType::Int16:
            case FieldType::Int32: {
                const int w = (fd.type == FieldType::Int8) ? 1 : (fd.type == FieldType::Int16 ? 2 : 4);
                const auto bo = fd.byteOrder.value_or(ByteOrder::Big);
                v = static_cast<std::uint64_t>(decode_int(raw, fd.offset, w, bo));
                break;
            }
            case FieldType::Bool: {
                const std::uint64_t one = 1;
                const std::uint64_t zero = 0;
                v = raw[fd.offset] ? one : zero;
                break;
            }
            case FieldType::Bitfield: {
                const int w = static_cast<int>(fd.length == 0 ? 1 : fd.length);
                const auto bo = fd.byteOrder.value_or(ByteOrder::Big);
                v = decode_uint(raw, fd.offset, w, bo);
                break;
            }
            case FieldType::Float32: {
                if (fd.offset + 4 > raw.size())
                    return make_err<ParsedFrame>(ErrorCode::PROTO_FIELD_INVALID, "Float32 越界: " + fd.name);
                std::uint32_t u32 = static_cast<std::uint32_t>(
                    decode_uint(raw, fd.offset, 4, fd.byteOrder.value_or(ByteOrder::Big)));
                float f;
                std::memcpy(&f, &u32, sizeof(f));
                v = static_cast<double>(f);
                break;
            }
            case FieldType::Double: {
                if (fd.offset + 8 > raw.size())
                    return make_err<ParsedFrame>(ErrorCode::PROTO_FIELD_INVALID, "Double 越界: " + fd.name);
                std::uint64_t u64 = decode_uint(raw, fd.offset, 8, fd.byteOrder.value_or(ByteOrder::Big));
                double d;
                std::memcpy(&d, &u64, sizeof(d));
                v = d;
                break;
            }
            case FieldType::Ascii: {
                const auto len = std::min(fd.length, raw.size() - fd.offset);
                v = std::string(reinterpret_cast<const char*>(raw.data() + fd.offset), len);
                break;
            }
            case FieldType::Hex:
            case FieldType::Bytes: {
                const auto len = std::min(fd.length, raw.size() - fd.offset);
                Bytes b(raw.begin() + fd.offset, raw.begin() + fd.offset + len);
                v = std::move(b);
                break;
            }
        }
        out.fields.emplace(fd.name, std::move(v));
    }

    return make_ok(std::move(out));
}

// ---------------------------------------------------------------------------
// 组帧编码
// ---------------------------------------------------------------------------
Result<Bytes> BasicProtocolEngine::encodeFrame(const ProtocolSchema& schema,
                                               const std::vector<FieldValue>& values) {
    if (values.size() != schema.fields.size()) {
        return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID,
                               "字段值数量不匹配 schema.fields");
    }

    // Step 1: 计算最小需要容纳的帧尾部位置（考虑所有 FieldDef.offset + length）
    std::size_t maxFieldEnd = 0;
    for (const auto& fd : schema.fields) {
        const std::size_t w = [&]() -> std::size_t {
            switch (fd.type) {
                case FieldType::Uint8: case FieldType::Int8: case FieldType::Bool: return 1;
                case FieldType::Uint16: case FieldType::Int16: return 2;
                case FieldType::Uint32: case FieldType::Int32: case FieldType::Float32: return 4;
                case FieldType::Double: return 8;
                case FieldType::Ascii: case FieldType::Bytes: case FieldType::Hex: return fd.length;
                case FieldType::Bitfield: return std::max<std::size_t>(1, fd.length);
            }
            return 1;
        }();
        const std::size_t end = fd.offset + w;
        if (end > maxFieldEnd) maxFieldEnd = end;
    }

    // Step 2: 构造 frame 骨架
    Bytes frame;
    // 2.1 startPattern
    if (!schema.frameDef.startPattern.empty()) {
        frame.insert(frame.end(), schema.frameDef.startPattern.begin(),
                     schema.frameDef.startPattern.end());
    }
    // 2.2 预留长度字段占位（Variable）
    bool hasVariableLen = false;
    std::size_t varLenStart = 0;
    std::size_t varLenWidth = 0;
    ByteOrder varLenBo = ByteOrder::Big;
    if (schema.lengthType == LengthType::Variable && schema.frameDef.lengthField) {
        const auto& lf = *schema.frameDef.lengthField;
        const std::size_t needEnd = lf.offset + lf.width;
        if (frame.size() < needEnd) frame.resize(needEnd, 0x00);
        hasVariableLen = true;
        varLenStart = lf.offset;
        varLenWidth = lf.width;
        varLenBo = lf.byteOrder;
    }
    // 2.3 扩展到至少容纳 maxFieldEnd（FieldDef.offset 是相对 frame 起始）
    if (frame.size() < maxFieldEnd) frame.resize(maxFieldEnd, 0x00);
    // 2.4 FixedLength：如果 schema 指定且更大，对齐
    if (schema.lengthType == LengthType::Fixed && schema.frameDef.fixedLength) {
        const auto len = *schema.frameDef.fixedLength;
        if (frame.size() < len) {
            frame.resize(len, 0x00);
        }
    }

    // Step 3: 按 FieldDef.offset 直接写入字段到 frame
    for (std::size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& fd = schema.fields[i];
        const auto& v = values[i];
        switch (fd.type) {
            case FieldType::Uint8:
            case FieldType::Uint16:
            case FieldType::Uint32:
            case FieldType::Int8:
            case FieldType::Int16:
            case FieldType::Int32:
            case FieldType::Bool:
            case FieldType::Bitfield: {
                const int w = [&]() -> int {
                    switch (fd.type) {
                        case FieldType::Uint8: case FieldType::Int8: case FieldType::Bool: return 1;
                        case FieldType::Uint16: case FieldType::Int16: return 2;
                        case FieldType::Uint32: case FieldType::Int32: return 4;
                        default: return static_cast<int>(fd.length == 0 ? 1 : fd.length);
                    }
                }();
                const auto bo = fd.byteOrder.value_or(ByteOrder::Big);
                const auto u = std::get_if<std::uint64_t>(&v);
                if (!u) return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID, "整型字段需要 uint64 值: " + fd.name);
                if (fd.type == FieldType::Int8 || fd.type == FieldType::Int16 || fd.type == FieldType::Int32) {
                    encode_int(frame, fd.offset, w, bo, static_cast<std::int64_t>(*u));
                } else {
                    encode_uint(frame, fd.offset, w, bo, *u);
                }
                break;
            }
            case FieldType::Float32: {
                const auto* d = std::get_if<double>(&v);
                if (!d) return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID, "Float32 需要 double 值");
                float f = static_cast<float>(*d);
                std::uint32_t u;
                std::memcpy(&u, &f, sizeof(u));
                encode_uint(frame, fd.offset, 4, fd.byteOrder.value_or(ByteOrder::Big), u);
                break;
            }
            case FieldType::Double: {
                const auto* d = std::get_if<double>(&v);
                if (!d) return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID, "Double 需要 double 值");
                std::uint64_t u;
                std::memcpy(&u, d, sizeof(u));
                encode_uint(frame, fd.offset, 8, fd.byteOrder.value_or(ByteOrder::Big), u);
                break;
            }
            case FieldType::Ascii: {
                const auto* s = std::get_if<std::string>(&v);
                if (!s) return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID, "Ascii 需要 string 值");
                const auto len = std::min(fd.length, s->size());
                std::copy_n(s->begin(), len, frame.begin() + fd.offset);
                break;
            }
            case FieldType::Bytes:
            case FieldType::Hex: {
                const auto* b = std::get_if<Bytes>(&v);
                if (!b) return make_err<Bytes>(ErrorCode::PROTO_ENCODE_INVALID, "Bytes 需要 Bytes 值");
                const auto len = std::min(fd.length, b->size());
                std::copy_n(b->begin(), len, frame.begin() + fd.offset);
                break;
            }
        }
    }

    // Step 4: endPattern
    if (!schema.frameDef.endPattern.empty()) {
        frame.insert(frame.end(), schema.frameDef.endPattern.begin(),
                     schema.frameDef.endPattern.end());
    }

    // Step 5: 回填长度字段（Variable）
    if (hasVariableLen) {
        const std::uint64_t len = static_cast<std::uint64_t>(frame.size() - (varLenStart + varLenWidth));
        encode_uint(frame, varLenStart, static_cast<int>(varLenWidth), varLenBo, len);
    }

    // Step 6: CRC
    if (schema.frameDef.crc.type != CrcType::None) {
        if (schema.frameDef.crc.type == CrcType::Crc8) {
            const std::uint8_t c = crc8(frame);
            if (schema.frameDef.crc.position == CrcPosition::Tail) frame.push_back(c);
            else frame.insert(frame.begin(), c);
        } else {
            const std::uint16_t c = crc16(frame);
            Bytes crcBytes(2);
            encode_uint(crcBytes, 0, 2, ByteOrder::Big, c);
            if (schema.frameDef.crc.position == CrcPosition::Tail) {
                frame.insert(frame.end(), crcBytes.begin(), crcBytes.end());
            } else {
                frame.insert(frame.begin(), crcBytes.begin(), crcBytes.end());
            }
        }
    }

    return make_ok(std::move(frame));
}

}  // namespace portpilot::core
