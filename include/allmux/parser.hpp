#pragma once

#include "allmux/model.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace allmux {

[[nodiscard]]
std::vector<SshHost> ssh_hosts(const std::filesystem::path& path, const std::span<std::string> active_sessions);
[[nodiscard]]
std::vector<DockerContainer> docker_containers(std::span<std::string> active_sessions);
[[nodiscard]]
std::vector<TmuxSession> tmux_paths_and_sessions(std::span<std::string> active_sessions);
[[nodiscard]]
AppData load_app_data(std::span<std::string> active_sessions);

} // namespace allmux
