#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "cpp_lab/storage/record.h"

namespace cpp_lab::storage {

enum class DecodeError : std::uint8_t {
    None = 0,

    HeaderTooShort,
    UnsupportedVersion,
    InvalidRecordType,

    KeyTooLarge,
    ValueTooLarge,

    TruncatedKey,
    TruncatedValue,

    InvalidDeleteValue,
    TrailingBytes,
};

struct DecodeResult {
    std::optional<Record> record;
    DecodeError error{DecodeError::None};
    [[nodiscard]] bool ok() const noexcept {
        return record.has_value();
    }
};

/**
 * WAL 单条记录编解码器。
 *
 * RecordCodec 不保存任何状态，因此只提供静态函数，
 * 并禁止创建 RecordCodec 对象。
 */

class RecordCodec {
    RecordCodec() = delete;
    static constexpr std::uint8_t kVersion{1};
    static constexpr std::size_t kHeaderSize{10};

    static constexpr std::size_t kMaxKeySize{1U << 20};
    static constexpr std::size_t kMaxValueSize{16U << 20};
    /**
     * 将一条 Record 编码成独立拥有数据的字节数组。
     *
     * 可能抛出：
     * - std::invalid_argument：Record 状态非法；
     * - std::length_error：key 或 value 超过限制；
     * - std::bad_alloc：内存分配失败。
     */
    [[nodiscard]] static std::vector<std::uint8_t> encode(const Record& record);

    /**
     * 从一段恰好包含一条完整记录的字节流中解码 Record。
     *
     * bytes 是不拥有数据的只读视图，本函数不会保存该 span。
     * 解码成功后，返回的 Record 拥有自己的 key/value 数据。
     */

    [[nodiscard]] static DecodeResult decode(std::span<const std::uint8_t> bytes);
    std::uint32_t shuff;
};
}  // namespace cpp_lab::storage
