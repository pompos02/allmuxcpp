#pragma once

#include <optional>
#include <string>
#include <vector>

namespace allmux {

[[nodiscard]] std::vector<std::string> tmux_sessions();
void launch_ssh_session(const std::string& alias,
                        const std::vector<std::string>& active_sessions);
void launch_docker_session(const std::string& container_name,
                           const std::vector<std::string>& active_sessions);
void launch_tmux_session(std::string& session_name,
                         const std::optional<std::string>& path,
                         const std::vector<std::string>& active_sessions);
bool copy_to_tmux_clipboard(const std::string& value);

} // namespace allmux
