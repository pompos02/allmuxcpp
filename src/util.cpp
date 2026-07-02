#include "allmux/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace allmux {

std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return s;
}

std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::filesystem::path home_dir() {
    if (const char* home = std::getenv("HOME")) {
        return home;
    }
    return ".";
}

std::filesystem::path config_dir() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        return xdg;
    }
    return home_dir() / ".config";
}

std::filesystem::path cache_allmux_dir() {
    auto path = home_dir() / ".cache" / "allmux";
    std::filesystem::create_directories(path);
    return path;
}


std::filesystem::path history_file() {
    return cache_allmux_dir() / "history.tsv";
}

std::filesystem::path theme_file() {
    return cache_allmux_dir() / "color_variant";
}


bool contains(const std::vector<std::string>& values, std::string_view value) {
    return std::ranges::find(values, value) != values.end();
}

std::string join_fields(const std::vector<std::string>& fields) {
    std::string out;
    for (const auto& field : fields) {
        if (!out.empty()) {
            out += ' ';
        }
        out += field;
    }
    return out;
}

std::string vector_strings_to_string(const std::vector<std::string>& values)
{
    std::string result = "[";

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            result += ',';

        result += '"';
        result += values[i];
        result += '"';
    }
    result += ']';

    return result;
}

} // namespace allmux
