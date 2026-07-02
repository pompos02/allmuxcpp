#pragma once

#include <filesystem>
#include <sstream>
#include <string_view>

namespace logger
{
bool enabled();
std::filesystem::path log_path();
void log_message(std::string_view file, int line, std::string_view function,
                 std::string_view message);
template <typename... Args>
void log(std::string_view file, int line, std::string_view function, Args&&... args)
{
    if (!enabled())
        return;

    std::ostringstream stream;
    (stream << ... << args);
    std::string basename = std::filesystem::path(file).filename().string();

    log_message(basename, line, function, stream.str());

}
} // namespace logger

#define ALLMUX_LOG(...) \
    ::logger::log(__FILE__, __LINE__, __func__, __VA_ARGS__)
