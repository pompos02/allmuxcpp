#pragma once

#include "allmux/model.hpp"

#include <filesystem>
#include <vector>

namespace allmux {

[[nodiscard]]
std::vector<SshHost> parse_ssh_config(const std::filesystem::path& path);
[[nodiscard]]
std::vector<DockerContainer> parse_docker_containers();
[[nodiscard]]
std::vector<TmuxSession> tmux_paths_and_sessions();
[[nodiscard]]
AppData load_app_data();

} // namespace allmux
