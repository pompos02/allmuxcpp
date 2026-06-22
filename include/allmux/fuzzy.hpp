#pragma once

#include <span>
#include <string>

namespace allmux {

struct FuzzyMatch {
    std::span<std::size_t> matched_indices{};
    bool matched = false;
    int score = 0;
};

[[nodiscard]]
FuzzyMatch fuzzy_match(const std::string& text,
                       const std::string& query,
                       std::span<std::size_t> out_buffer);

} // namespace allmux
