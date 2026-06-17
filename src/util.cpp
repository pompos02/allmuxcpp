#include "allmux/util.hpp"

#include <algorithm>
#include <cstdlib>

namespace allmux {

std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string shell_quote(std::string_view value) {
    std::string quoted = "'";
    for (const char ch : value) {
        quoted += ch == '\'' ? "'\\''" : std::string(1, ch);
    }
    quoted += "'";
    return quoted;
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

} // namespace allmux
