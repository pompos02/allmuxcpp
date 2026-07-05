#include "allmux/fuzzy.hpp"
#include "allmux/util.hpp"
#include <algorithm>
#include <cstddef>
#include <rapidfuzz/fuzz.hpp>
#include <vector>


namespace allmux
{
namespace 
{
std::vector<std::string_view> split_tokens(std::string_view path)
{
    std::vector<std::string_view> parts;
    std::size_t start{0};
    while (start < path.size())
    {
        while (start < path.size() && path[start] == '/') ++start;

        if (start == path.size()) break;

        std::size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();

        parts.emplace_back(path.data() + start, end - start);
        start = end;
    }

    return parts;
}
}

[[nodiscard]]
std::span<std::size_t> matched_indices(std::string_view text,
                                       std::string_view query,
                                       std::span<std::size_t> out_buffer)
{
    if (query.empty() || out_buffer.empty())
    {
        return out_buffer.subspan(0, 0);
    }

    // Prefer an exact contiguous case-insensitive match.
    for (std::size_t i = 0; i + query.size() <= text.size(); ++i)
    {
        bool matches = true;
        for (std::size_t j = 0; j < query.size(); ++j)
        {
            if (text[i + j] != query[j])
            {
                matches = false;
                break;
            }
        }

        if (matches)
        {
            const auto count = std::min(query.size(), out_buffer.size());
            for (std::size_t j = 0; j < count; ++j)
            {
                out_buffer[j] = i + j;
            }
            return out_buffer.subspan(0, count);
        }
    }

    // Otherwise highlight the first ordered subsequence match.
    std::size_t write_idx = 0;
    std::size_t query_idx = 0;

    for (std::size_t i = 0; i < text.size() && query_idx < query.size(); ++i)
    {
        if (text[i] == query[query_idx])
        {
            if (write_idx >= out_buffer.size())
            {
                break;
            }

            out_buffer[write_idx++] = i;
            ++query_idx;
        }
    }

    // Only return subsequence highlights if the whole query was found.
    if (query_idx != query.size())
    {
        return out_buffer.subspan(0, 0);
    }

    return out_buffer.subspan(0, write_idx);
}

[[nodiscard]] FuzzyMatch fuzzy_match(const std::string& text,
                                     const std::string& query,
                                     std::span<std::size_t> out_buffer)
                                     
{
    if (query.empty())
        return {.matched_indices = {}, .matched = true, .score = 0};

    const auto lower_text = str_tolower(text);
    const auto lower_query = str_tolower(query);

    auto indices = matched_indices(lower_text, lower_query, out_buffer);
    if (indices.empty()) return {};

    int score = rapidfuzz::fuzz::partial_ratio(lower_query, lower_text);
    if (score < 60) return {};

    if (lower_text.contains(lower_query)) score += 20;
    if (lower_text.starts_with(lower_query)) score += 20;

    for (auto token : split_tokens(lower_text))
    {
        if (token.starts_with(lower_query))
        {
            score += 15;
            break;
        }
    }

    return {
        .matched_indices = indices,
        .matched = true,
        .score = score,
    };
}
} // namespace allmux
