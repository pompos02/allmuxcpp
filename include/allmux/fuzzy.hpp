#pragma once

#include <string>

namespace allmux {

struct FuzzyMatch {
    bool matched = false;
    int score = 0;
};

[[nodiscard]]
FuzzyMatch fuzzy_match(const std::string& text, const std::string& query);

} // namespace allmux
