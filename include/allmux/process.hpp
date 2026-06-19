#pragma once

#include <span>
#include <string>
#include <vector>

namespace allmux {

struct CommandResult {
    int exit_code = 1;
    std::string output;
};

[[nodiscard]] CommandResult run_command(const std::span<std::string> command);
[[nodiscard]] int run_status(const std::string& command);

} // namespace allmux
