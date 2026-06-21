#include "allmux/parser.hpp"

#include "allmux/process.hpp"
#include "allmux/util.hpp"
#include "allmux/tmux.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace allmux {
namespace fs = std::filesystem;

static void
push_tmux_dir(std::vector<std::pair<std::string, std::string>>& dirs,
              std::set<fs::path>& seen, const fs::path& path) {
    const auto canonical = fs::canonical(path);
    if (seen.insert(canonical).second) {
        dirs.emplace_back(canonical.string(), canonical.filename().string());
    }
}

static std::vector<std::pair<std::string, std::string>> tmux_dirs() {
    const fs::path file = config_dir() / ".allmux";
    std::ifstream input(file);
    if (!input) {
        throw std::runtime_error("could not read " + file.string());
    }

    std::vector<std::pair<std::string, std::string>> dirs;
    std::set<fs::path> seen;
    for (std::string line; std::getline(input, line);) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const fs::path full_path = home_dir() / line;
        std::error_code ec;
        if (!fs::is_directory(full_path, ec)) {
            continue;
        }

        push_tmux_dir(dirs, seen, full_path);
        for (const auto& child : fs::directory_iterator(full_path, ec)) {
            if (ec) {
                break;
            }
            const auto name = child.path().filename().string();
            if (!name.starts_with(".") && child.is_directory(ec)) {
                push_tmux_dir(dirs, seen, child.path());
            }
        }
    }
    return dirs;
}

std::vector<TmuxSession> tmux_paths_and_sessions() {
    const auto dirs = tmux_dirs();
    const auto active = tmux_sessions();
    std::set<std::string> path_names;
    std::vector<TmuxSession> sessions;

    for (const auto& [full_path, name] : dirs) {
        path_names.insert(name);
        sessions.push_back({.full_path = full_path,
                            .session_name = name,
                            .is_active = contains(active, name)});
    }
    for (const auto& name : active) {
        if (!path_names.contains(name)) {
            sessions.push_back({.full_path = std::nullopt,
                                .session_name = name,
                                .is_active = true});
        }
    }
    return sessions;
}

std::vector<SshHost> parse_ssh_config(const fs::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error(
            std::format("failed to read ssh config: {}", path.string()));
    }

    auto lowercase = [](std::string s) {
        std::ranges::transform(s, s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    };

    auto append_description = [](std::optional<std::string>& desc,
                                 const std::string& text) {
        desc.emplace(desc.value_or("") + std::string{trim(text)} + '\n');
    };

    std::vector<SshHost> hosts;
    std::optional<std::string> description;

    for (std::string raw; std::getline(input, raw);) {
        const auto line = trim(raw);

        if (line.empty())
            continue;

        if (line.starts_with('#')) {
            append_description(description, line.substr(1));
        }

        std::istringstream stream{std::string{line}};
        std::string key;
        stream >> key;
        key = lowercase(std::move(key));

        if (key == "host") {
            std::string alias;
            stream >> alias;
            if (alias == "*")
                continue;

            if (!alias.empty() && alias != "") {
                hosts.emplace_back(alias, std::string{}, std::string{},
                                   std::exchange(description, std::nullopt),
                                   false);
            }
            continue;
        }

        if (hosts.empty())
            continue;

        if (key == "hostname") {
            stream >> hosts.back().hostname;
        } else if (key == "user") {
            stream >> hosts.back().user;
        }
    }
    return hosts;
}

std::vector<DockerContainer> docker_containers() {
    const char* command[] = {
        "docker", "ps", "-a",
        "--format", "{{.Names}}\t{{.Status}}"
    };

    const auto result = run_command(command);
    std::vector<DockerContainer> containers;
    if (result.exit_code != 0) {
        return containers;
    }

    std::istringstream stream(result.output);

    for (std::string line; std::getline(stream, line);) {
        if (trim(line).empty()) {
            continue;
        }
        DockerContainer container{};
        std::size_t separator = line.find('\t');
        if ( separator == std::string::npos) continue;

        container.name = line.substr(0, separator);
        container.status = line.substr(separator + 1).contains("Up");
        containers.push_back(std::move(container));
    }
    return containers;
}

AppData load_app_data() {
    return {.hosts = parse_ssh_config(home_dir() / ".ssh" / "config"),
            .containers = docker_containers(),
            .tmux_sessions = tmux_paths_and_sessions()};
}

} // namespace allmux
