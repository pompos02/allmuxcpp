#include "allmux/fuzzy.hpp"
#include <cmath>
#include <rapidfuzz/fuzz.hpp>

namespace allmux {

FuzzyMatch fuzzy_match(const std::string& text, const std::string& query) {
    if (query.empty()) {
        return {.matched = true, .score = 0};
    }

    const int score = rapidfuzz::fuzz::partial_ratio(query, text);
    if (score < 60) {
        return {};
    }
    return {.matched = true, .score = static_cast<int>(std::lround(score))};
}
} // namespace allmux
