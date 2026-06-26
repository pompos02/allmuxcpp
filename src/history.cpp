#include "allmux/history.hpp"
#include "allmux/util.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <ranges>
#include <charconv>

namespace allmux {

auto unix_timestamp() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

namespace {

namespace fs = std::filesystem;

constexpr double DECAY_CONSTANT = 0.0693; // ln(2)/10 for a 10-day half-life.
constexpr std::int64_t SECONDS_PER_DAY = 86400;
constexpr std::int16_t MAX_HISTORY_DAYS = 30;
constexpr std::size_t MAX_TIMESTAMPS_PER_ENTRY = 128;

void parse_history_line(History& history, std::string_view line)
{
    std::size_t tab{line.find('\t')};
    if (tab == std::string_view::npos)
    {
        // TODO: maybe handle this more elegantly
        throw std::runtime_error("invalid history line");
    }

    auto key = std::string{line.substr(0, tab)};
    auto& timestamps = history.entries[key];

    auto values = line.substr(tab + 1);

    while(!values.empty())
    {
        auto comma = values.find(',');

        std::string_view token =
            (comma == std::string_view::npos) ? values : values.substr(0, comma);

        if (!token.empty())
        {
            std::int64_t value{};
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);

            if (ec == std::errc{} && ptr == (token.data() + token.size()))
            {
                timestamps.push_back(value);
            }
        }
        
        if (comma == std::string_view::npos)
        {
            break;
        }

        values.remove_prefix(comma+1);
    }
}

void write_to_history_file(std::unordered_map<std::string, std::deque<std::int64_t>>& entries) {
    auto file = history_file();
    auto parent = file.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        throw std::runtime_error("Failed to create cache dir" +
                                 parent.string());
    }

    std::vector<std::string> lines;
    lines.reserve(entries.size());
    for (const auto& [key, timestamps] : entries) {
        std::string line = key;
        line += "\t";

        bool first = true;
        for (const std::int64_t timestamp : timestamps) {
            if (!first) {
                line += ',';
            }
            first = false;
            line += std::to_string(timestamp);
        }
        lines.push_back(std::move(line));
    }

    std::ranges::sort(lines);

    std::ofstream out(file, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        // throw std::runtime_error("failed to open history file: " + file.string());
        return;
    }

    for (const auto& line : lines) {
        out << line << '\n';
    }

    if (!out) {
        throw std::runtime_error("failed to writing to history file: " + file.string());
    }
}

} // namespace

auto History::load_history() -> History {
    auto file = history_file();
    if (!fs::exists(file)) {
        return {};
    }

    std::ifstream f(file);
    if (!f.is_open()) {
        throw std::runtime_error("failed to open: " + file.string());
    }

    History history;
    std::string line{};
    while (std::getline(f, line)) {
        parse_history_line(history, line);
    }

    return history;
}

void History::record_access(std::string_view key) {
    auto now = unix_timestamp();
    auto cutoff_time = now - MAX_HISTORY_DAYS * SECONDS_PER_DAY;

    auto& times = entries[std::string{key}];

    while (!times.empty()) {
        auto front_time = times.front();

        if (front_time < cutoff_time || times.size() >= MAX_TIMESTAMPS_PER_ENTRY) {
            times.pop_front();
        } else {
            break;
        }
    }

    times.push_back(now);
    write_to_history_file(entries);
}

int64_t History::score(std::string_view key) const {
    auto it = entries.find(std::string{key});
    if (it == entries.end()) {
        return 0;
    }

    auto const& timestamps = it->second;

    if (timestamps.empty()) {
        // TODO: notify the user?
        return 0;
    }
    auto now = unix_timestamp();
    std::int64_t cutoff_time = now - MAX_HISTORY_DAYS * SECONDS_PER_DAY;
    double total_frecency{0.0};

    for (auto ts : timestamps | std::views::reverse) {
        if (ts < cutoff_time) {
            break;
        }

        double days_ago = (now - static_cast<double>(ts)) / SECONDS_PER_DAY;
        double decay_factor = std::exp(-DECAY_CONSTANT * days_ago);
        total_frecency += decay_factor;
    }

    double normalized_frecency;
    if (total_frecency <= 10) {
        normalized_frecency = total_frecency;
    } else {
        normalized_frecency = 10. * std::sqrt(total_frecency - 10.);
    }

    return static_cast<int64_t>(normalized_frecency);
}

} // namespace allmux
