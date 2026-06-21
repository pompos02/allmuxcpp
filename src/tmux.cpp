#include "allmux/tmux.hpp"

#include "allmux/process.hpp"
#include "allmux/util.hpp"

#include <stdexcept>
#include <sstream>

namespace allmux {

std::vector<std::string> tmux_sessions() {
    const char* command[] = {"tmux", "list-sessions", "-F", "#{session_name}"};
    const auto result = run_command(command);
    if (result.exit_code != 0) {
        throw std::runtime_error(std::format(
            "failed to list tmux sessions:  {}", trim(result.output)));
    }

    std::vector<std::string> sessions;
    std::istringstream stream(result.output);
    for (std::string line; std::getline(stream, line);) {
        line = trim(line);
        if (!line.empty()) {
            sessions.push_back(line);
        }
    }
    return sessions;
}

static std::string new_session_at(const std::string& name,
                                  const std::optional<std::string>& path) {
    const auto start_path = path.value_or(home_dir().string());

    std::string quoted_name = shell_quote(name);
    std::string quoted_path = shell_quote(start_path);

    const char* command[] = {
        "tmux",
        "new-session",
        "-d",
        "-P",
        "-F",
        "'#{session_name}:#{window_index}.#{pane_index}'",
        "-s",
        quoted_name.c_str(),
        "-c",
        quoted_path.c_str(),
    };

    const auto result = run_command(command);
    if (result.exit_code != 0) {
        throw std::runtime_error("tmux new-session failed: " +
                                 trim(result.output));
    }
    return trim(result.output);
}

static void send_keys(const std::string& pane_target,
                      const std::string& command) {
    const auto status = run_status("tmux send-keys -t " +
                                   shell_quote(pane_target) + " " +
                                   shell_quote(command) + " C-m");
    if (status != 0) {
        throw std::runtime_error("tmux send-keys failed");
    }
}

static void goto_session(const std::string& name) {
    if (run_status("tmux switch-client -t " + shell_quote(name)) != 0) {
        throw std::runtime_error("failed to go to tmux session");
    }
}

void launch_ssh_session(const std::string& alias,
                        const std::vector<std::string>& active_sessions) {
    if (!contains(active_sessions, alias)) {
        send_keys(new_session_at(alias, std::nullopt), "ssh " + alias);
    }
    goto_session(alias);
}

void launch_docker_session(const std::string& container_name,
                           const std::vector<std::string>& active_sessions) {
    if (!contains(active_sessions, container_name)) {
        send_keys(new_session_at(container_name, std::nullopt),
                  "docker exec -it " + container_name + " bash");
    }
    goto_session(container_name);
}

void launch_tmux_session(const std::string& session_name,
                         const std::optional<std::string>& path,
                         const std::vector<std::string>& active_sessions) {
    if (!contains(active_sessions, session_name)) {
        new_session_at(session_name, path);
    }
    goto_session(session_name);
}

bool copy_to_tmux_clipboard(const std::string& value) {
    return run_status("printf %s " + shell_quote(value) +
                      " | tmux load-buffer -w -") == 0;
}

} // namespace allmux
