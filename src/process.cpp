#include "allmux/process.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace allmux {

CommandResult run_command(const std::span<std::string> args) {
    CommandResult result;

    if (args.empty()) {
        result.exit_code = 1;
        result.output = "empty command";
        return result;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        result.exit_code = -1;
        result.output = std::strerror(errno);
        return result;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    std::vector<char*> argv;
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }

    argv.push_back(nullptr);

    pid_t pid;

    int rc =
        posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);

    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (rc != 0) {
        close(pipefd[0]);
        result.exit_code = rc;
        result.output = std::strerror(rc);
        return result;
    }

    std::array<char, 4096> buffer{};
    ssize_t n;

    while ((n = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
        result.output.append(buffer.data(), n);
    }

    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = 1;
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
