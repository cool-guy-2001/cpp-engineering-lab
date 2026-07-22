#pragma once

#include <cstdint>
#include <string>

namespace cpp_lab::storage {

enum class RecordType : std::uint8_t {
    Put = 1,
    Delete = 2,
};

struct Record {
    RecordType type{RecordType::Put};
    std::string key;
    std::string value;

    bool operator==(const Record&) const = default;
};
}  // namespace cpp_lab::storage
