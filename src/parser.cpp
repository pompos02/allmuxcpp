#include "allmux/parser.hpp"

#include "allmux/process.hpp"
#include "allmux/tmux.hpp"
#include "allmux/util.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

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

static std::string format_permissions(const fs::path& path) {
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0) {
        return "----------";
    }

    std::string permissions;
    permissions +=
        S_ISDIR(st.st_mode) ? 'd' : (S_ISLNK(st.st_mode) ? 'l' : '-');
    for (const mode_t bit : {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                             S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH}) {
        if ((st.st_mode & bit) == 0) {
            permissions += '-';
        } else if (bit == S_IRUSR || bit == S_IRGRP || bit == S_IROTH) {
            permissions += 'r';
        } else if (bit == S_IWUSR || bit == S_IWGRP || bit == S_IWOTH) {
            permissions += 'w';
        } else {
            permissions += 'x';
        }
    }
    return permissions;
}

static std::string human_size(std::uintmax_t bytes) {
    const std::array<std::string, 5> units = {"B", "K", "M", "G", "T"};
    auto size = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (size >= 1024.0 && unit + 1 < units.size()) {
        size /= 1024.0;
        ++unit;
    }

    std::ostringstream out;
    if (unit == 0) {
        out << bytes << units[unit];
    } else {
        out.setf(std::ios::fixed);
        out.precision(size < 10.0 ? 1 : 0);
        out << size << units[unit];
    }
    return out.str();
}

static std::optional<std::string> tmux_ls_preview(const std::string& path) {
    std::vector<std::string> rows;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (ec) {
            return std::nullopt;
        }
        const auto size =
            entry.is_regular_file(ec) ? fs::file_size(entry.path(), ec) : 0;
        rows.push_back(format_permissions(entry.path()) + " " +
                       entry.path().filename().string() + " " +
                       human_size(size));
    }
    std::ranges::sort(rows, [](std::string left, std::string right) {
        std::ranges::transform(left, left.begin(), [](unsigned char ch) {
            return std::tolower(ch);
        });
        std::ranges::transform(right, right.begin(), [](unsigned char ch) {
            return std::tolower(ch);
        });
        return left < right;
    });

    std::ostringstream out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i != 0) {
            out << '\n';
        }
        out << rows[i];
    }
    return out.str();
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
                            .is_active = contains(active, name),
                            .preview = tmux_ls_preview(full_path)});
    }
    for (const auto& name : active) {
        if (!path_names.contains(name)) {
            sessions.push_back({.full_path = std::nullopt,
                                .session_name = name,
                                .is_active = true,
                                .preview = std::nullopt});
        }
    }
    return sessions;
}

std::vector<SshHost> parse_ssh_config(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read ssh config: " + path.string());
    }

    std::vector<SshHost> hosts;
    std::optional<std::string> current_description;
    for (std::string raw; std::getline(input, raw);) {
        const auto line = trim(raw);
        if (line.empty()) {
            continue;
        }
        if (line.starts_with("#")) {
            current_description =
                current_description.value_or("") + trim(line.substr(1)) + "\n";
            continue;
        }

        std::istringstream stream(line);
        std::string key;
        stream >> key;
        std::ranges::transform(key, key.begin(), [](unsigned char ch) {
            return std::tolower(ch);
        });
        if (key == "host") {
            std::string alias;
            stream >> alias;
            if (!alias.empty() && alias != "*") {
                hosts.push_back({.alias = alias,
                                 .hostname = {},
                                 .user = {},
                                 .description = current_description,
                                 .is_active_tmux = false});
                current_description.reset();
            }
        } else if (key == "hostname" && !hosts.empty()) {
            stream >> hosts.back().hostname;
        } else if (key == "user" && !hosts.empty()) {
            stream >> hosts.back().user;
        }
    }

    const auto sessions = tmux_sessions();
    for (auto& host : hosts) {
        host.is_active_tmux = contains(sessions, host.alias);
    }
    return hosts;
}

static std::string column_field(const std::string& line,
                                const std::vector<std::size_t>& starts,
                                std::size_t index) {
    if (index >= starts.size() || starts[index] >= line.size()) {
        return {};
    }
    const auto end =
        index + 1 < starts.size() ? starts[index + 1] : line.size();
    return trim(
        line.substr(starts[index], std::min(end, line.size()) - starts[index]));
}

std::vector<DockerContainer> parse_docker_containers() {
    std::string command[] = {"docker", "ps", "-a"};
    const auto result = run_command(command);
    std::vector<DockerContainer> containers;
    if (result.exit_code != 0) {
        return containers;
    }

    std::istringstream stream(result.output);
    std::string header;
    if (!std::getline(stream, header)) {
        return containers;
    }

    const std::array<std::string, 7> columns = {
        "CONTAINER ID", "IMAGE", "COMMAND", "CREATED",
        "STATUS",       "PORTS", "NAMES"};
    std::vector<std::size_t> starts;
    for (const auto& column : columns) {
        if (const auto pos = header.find(column); pos != std::string::npos) {
            starts.push_back(pos);
        }
    }

    for (std::string line; std::getline(stream, line);) {
        if (trim(line).empty()) {
            continue;
        }
        DockerContainer container;
        container.id = column_field(line, starts, 0);
        container.image = column_field(line, starts, 1);
        container.command = column_field(line, starts, 2);
        container.created_at = column_field(line, starts, 3);
        container.status_text = column_field(line, starts, 4);
        container.ports = column_field(line, starts, 5);
        container.name = column_field(line, starts, 6);
        container.status = container.status_text.contains("Up");
        containers.push_back(std::move(container));
    }

    const auto sessions = tmux_sessions();
    for (auto& container : containers) {
        container.is_active_tmux = contains(sessions, container.name);
    }
    return containers;
}

AppData load_app_data() {
    return {.hosts = parse_ssh_config(home_dir() / ".ssh" / "config"),
            .containers = parse_docker_containers(),
            .tmux_sessions = tmux_paths_and_sessions()};
}

} // namespace allmux
