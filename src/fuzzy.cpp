#include "allmux/fuzzy.hpp"

#include <algorithm>
#include <cctype>

namespace allmux {

FuzzyMatch fuzzy_match(const std::string& text, const std::string& query) {
    if (query.empty()) {
        return {.matched = true, .score = 0, .indices = {}};
    }

    auto lower_text = text;
    auto lower_query = query;
    std::ranges::transform(lower_text, lower_text.begin(),
                           [](unsigned char ch) { return std::tolower(ch); });
    std::ranges::transform(lower_query, lower_query.begin(),
                           [](unsigned char ch) { return std::tolower(ch); });

    std::size_t pos = 0;
    int score = 0;
    int streak = 0;
    std::vector<std::size_t> indices;

    for (const char wanted : lower_query) {
        const auto found = lower_text.find(wanted, pos);
        if (found == std::string::npos) {
            return {};
        }
        streak = found == pos ? streak + 1 : 1;
        score += 10 + streak * 4 - static_cast<int>(found - pos);
        indices.push_back(found);
        pos = found + 1;
    }

    return {.matched = true, .score = score, .indices = std::move(indices)};
}

} // namespace allmux
