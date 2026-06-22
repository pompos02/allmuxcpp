#include "allmux/fuzzy.hpp"
#include "allmux/util.hpp"
#include <cmath>
#include <rapidfuzz/fuzz.hpp>

namespace allmux {

[[nodiscard]]
std::span<std::size_t> matched_indices(std::string_view text,
                                       std::string_view query,
                                       std::span<std::size_t> out_buffer)
{
    if (query.empty() || out_buffer.empty()) {
        return out_buffer.subspan(0, 0);
    }

    // Prefer an exact contiguous case-insensitive match.
    for (std::size_t i = 0; i + query.size() <= text.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < query.size(); ++j) {
            if (text[i + j] != query[j]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            const auto count = std::min(query.size(), out_buffer.size());
            for (std::size_t j = 0; j < count; ++j) {
                out_buffer[j] = i + j;
            }
            return out_buffer.subspan(0, count);
        }
    }

    // Otherwise highlight the first ordered subsequence match.
    std::size_t write_idx = 0;
    std::size_t query_idx = 0;

    for (std::size_t i = 0; i < text.size() && query_idx < query.size(); ++i) {
        if (text[i] == query[query_idx]) {
            if (write_idx >= out_buffer.size()) {
                break;
            }

            out_buffer[write_idx++] = i;
            ++query_idx;
        }
    }

    // Only return subsequence highlights if the whole query was found.
    if (query_idx != query.size()) {
        return out_buffer.subspan(0, 0);
    }

    return out_buffer.subspan(0, write_idx);
}

[[nodiscard]] FuzzyMatch fuzzy_match(const std::string& text,
                                     const std::string& query,
                                     std::span<std::size_t> out_buffer)
{
    if (query.empty()) {
        return {.matched_indices = {}, .matched = true, .score = 0};
    }

    const int score = rapidfuzz::fuzz::partial_ratio(query, text);
    if (score < 60) {
        return {};
    }

    auto indices =
        matched_indices(str_tolower(text), str_tolower(query), out_buffer);

    return {.matched_indices = indices,
            .matched = true,
            .score = static_cast<int>(std::lround(score))};
}
} // namespace allmux
