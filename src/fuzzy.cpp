#include "allmux/fuzzy.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>


namespace allmux
{
namespace 
{
struct MatchRange
{
    std::size_t min = 0;
    std::size_t max = 0;
};

[[nodiscard]] bool char_is_space(char c)
{
    return c == ' ' || c == '\r' || c == '\t' || c == '\f' || c == '\v' ||
           c == '\n';
}

[[nodiscard]] char normalized_char(char c)
{
    if (c == '\\') return '/';
    if ('A' <= c && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
    return c;
}

[[nodiscard]] bool string_matches(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) return false;

    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (normalized_char(a[i]) != normalized_char(b[i])) return false;
    }

    return true;
}

[[nodiscard]] std::size_t find_needle(std::string_view haystack,
                                      std::size_t start_pos,
                                      std::string_view needle)
{
    if (needle.empty()) return std::min(start_pos, haystack.size());
    if (needle.size() > haystack.size()) return haystack.size();

    const auto last_pos = haystack.size() - needle.size();
    if (start_pos > last_pos) return haystack.size();

    for (std::size_t pos = start_pos; pos <= last_pos; ++pos)
    {
        if (string_matches(haystack.substr(pos, needle.size()), needle))
            return pos;
    }

    return haystack.size();
}

[[nodiscard]] std::span<std::size_t>
write_matched_indices(std::vector<MatchRange>& ranges,
                      std::span<std::size_t> out_buffer)
{
    std::ranges::sort(ranges, {}, &MatchRange::min);

    std::size_t count = 0;
    for (const MatchRange& range : ranges)
    {
        for (std::size_t pos = range.min; pos < range.max; ++pos)
        {
            if (count == out_buffer.size()) return out_buffer.subspan(0, count);
            out_buffer[count++] = pos;
        }
    }

    return out_buffer.subspan(0, count);
}
}

[[nodiscard]] FuzzyMatch fuzzy_match(const std::string& text,
                                      const std::string& query,
                                      std::span<std::size_t> out_buffer)
{
    if (query.empty())
        return {.matched_indices = {}, .matched = true, .score = 0};

    std::vector<MatchRange> ranges;
    std::size_t needle_part_count = 0;
    std::size_t total_dim = 0;

    for (std::size_t part_pos = 0; part_pos < query.size();)
    {
        while (part_pos < query.size() && char_is_space(query[part_pos]))
            ++part_pos;
        if (part_pos == query.size()) break;

        std::size_t part_end = part_pos;
        while (part_end < query.size() && !char_is_space(query[part_end]))
            ++part_end;

        const std::string_view part{query.data() + part_pos,
                                    part_end - part_pos};
        ++needle_part_count;

        std::size_t find_pos = 0;
        while (find_pos < text.size())
        {
            find_pos = find_needle(text, find_pos, part);
            const auto range = std::ranges::find_if(ranges, [&](const MatchRange& r) {
                return r.min <= find_pos && find_pos < r.max;
            });
            if (range == ranges.end()) break;
            find_pos = range->max;
        }

        if (find_pos < text.size())
        {
            ranges.push_back({.min = find_pos, .max = find_pos + part.size()});
            total_dim += part.size();
        }

        part_pos = part_end;
    }

    if (ranges.size() != needle_part_count)
        return {};

    const int score = text.empty()
                          ? 0
                          : static_cast<int>((total_dim * 100) / text.size());

    return {
        .matched_indices = write_matched_indices(ranges, out_buffer),
        .matched = true,
        .score = score,
    };
}
} // namespace allmux
