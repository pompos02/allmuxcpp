#pragma once

#include <span>
#include <string>

namespace allmux {

struct CommandResult {
    int exit_code = 1;
    std::string output;
};

[[nodiscard]]
CommandResult run_command(std::span<const char* const> args);
[[nodiscard]]
int run_status(const std::string& command);

} // namespace allmux
