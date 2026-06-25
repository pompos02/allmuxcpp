#pragma once

#include <cstdint>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <string>

namespace allmux {

[[nodiscard]] std::int64_t unix_timestamp();

struct History {
    std::unordered_map<std::string, std::deque<std::int64_t>> entries;

    static History load_history();
    void record_access(std::string_view key);
    int64_t score(std::string_view key);
};
} // namespace allmux
