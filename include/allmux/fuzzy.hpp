#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace allmux {

struct FuzzyMatch {
    bool matched = false;
    int score = 0;
    std::vector<std::size_t> indices;
};

[[nodiscard]] FuzzyMatch fuzzy_match(const std::string& text,
                                     const std::string& query);

} // namespace allmux
