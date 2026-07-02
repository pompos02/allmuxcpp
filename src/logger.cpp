#include "allmux/logger.hpp"
#include "allmux/util.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include <string_view>

#include <unistd.h>

namespace logger {
namespace {

std::mutex log_mutex;

std::string timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
    localtime_r(&time, &local_time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%H:%M:%S");
    stream << std::setw(3);

    return stream.str();
}

std::ofstream& log_file()
{
    static std::ofstream file = []{
        auto path = log_path();
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            throw std::runtime_error("Error creating" + path.parent_path().string());

        std::ofstream stream(path, std::ios::app);
        if (stream)
        {
            stream << "------------------------------------------------------------\n"
                   << timestamp() << " pid=" << getpid() << " logger started!\n";
            stream.flush();
        }
        return stream;
    }();

    return file;
}

} // namespace

bool enabled()
{
    const char* value = std::getenv("ALLMUX_DEBUG");
    return value != nullptr && std::string_view(value) != "0";
}

std::filesystem::path log_path()
{
    return allmux::home_dir() / ".traces" / "allmux" / "allmux_debug.log";
}

void log_message(std::string_view file, int line,
                 std::string_view function,
                 std::string_view message)
{
    if (!enabled())
        return;

    std::lock_guard lock(log_mutex);

    auto& file_stream = log_file();
    if (!file_stream)
        return;

    file_stream << timestamp()
                << '[' << file << ':' << line << ']'
                << '@' << function
                << " | " << message
                << '\n';

    file_stream.flush();
}

} // namespace allmux::logger
