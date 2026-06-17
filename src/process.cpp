#include "allmux/process.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>

namespace allmux {

CommandResult run_command(const std::string& command) {
    CommandResult result;
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr) {
        result.output = "failed to spawn command";
        return result;
    }

    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
           nullptr) {
        result.output += buffer.data();
    }

    const int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    return result;
}

int run_status(const std::string& command) {
    const int status = std::system(command.c_str());
    if (status == -1) {
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
}

} // namespace allmux
