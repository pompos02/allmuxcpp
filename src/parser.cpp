#include "allmux/parser.hpp"

#include "allmux/process.hpp"
#include "allmux/util.hpp"

#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace allmux {
namespace fs = std::filesystem;

struct TmuxDir {
    std::string full_path;
    std::string base_name;
};

static bool is_tmux_session(std::string_view name, std::span<const std::string> active_sesssions) {
    for (const auto& session : active_sesssions) {
        if (name == session)
            return true;
    }

    return false;
}

static std::vector<TmuxDir> tmux_dirs() {
    const fs::path file = config_dir() / ".allmux";
    std::ifstream input(file);
    if (!input) {
        throw std::runtime_error("could not read " + file.string());
    }

    std::vector<TmuxDir> dirs;
    std::set<fs::path> seen;
    for (std::string line; std::getline(input, line);) {
        line = trim(line);
        if (line.empty()) {
            line = ""; // this later becomes $HOME
        }

        const fs::path full_path = home_dir() / line;
        std::error_code ec;
        if (!fs::is_directory(full_path, ec)) {
            continue;
        }

        for (const auto& child : fs::directory_iterator(full_path, ec)) {
            if (ec) {
                continue;
            }
            const auto name = child.path().filename().string();
            auto path = child.path();
            if (!name.starts_with(".") && child.is_directory(ec) && !seen.contains(path)) {
                dirs.emplace_back(
                    TmuxDir{
                        .full_path = child.path(),
                        .base_name = name,
                    }
                );
                seen.insert(path);
            }
        }
    }
    return dirs;
}

std::vector<TmuxSession> tmux_paths_and_sessions(std::span<std::string> const active_sesssions) {
    const auto dirs = tmux_dirs();
    std::set<std::string> path_names;
    std::vector<TmuxSession> sessions;

    for (const auto& [full_path, name] : dirs) {
        path_names.insert(name);
        bool is_active = is_tmux_session(name, active_sesssions);
        sessions.push_back({.full_path = full_path,
                            .basename = name,
                            .is_active = is_active});
    }
    return sessions;
}

std::vector<SshHost> ssh_hosts(const fs::path& path,const std::span<std::string> active_sesssions) {
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
                bool is_active = is_tmux_session(alias, active_sesssions);
                hosts.emplace_back(alias, std::string{}, std::string{},
                                   std::exchange(description, std::nullopt),
                                   is_active);
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

std::vector<DockerContainer> docker_containers(std::span<std::string> const active_sesssions) {
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
        container.is_active = is_tmux_session(container.name, active_sesssions);

        containers.push_back(std::move(container));
    }
    return containers;
}

AppData load_app_data(std::span<std::string> active_sessions) {
    return {.hosts = ssh_hosts(home_dir() / ".ssh" / "config", active_sessions),
            .containers = docker_containers(active_sessions),
            .tmux_sessions = tmux_paths_and_sessions(active_sessions)};
}

template <class Fn>
auto submit(boost::asio::thread_pool& pool, Fn fn)
    -> std::future<std::invoke_result_t<Fn>> 
{
    using Result = std::invoke_result_t<Fn>;

    auto task = std::make_shared<std::packaged_task<Result()>>(std::move(fn));
    auto future = task->get_future();

    boost::asio::post(pool, [task] {
        (*task)();
    });

    return future;
}

AppData load_app_data_parallel(std::span<std::string> active_sesssions,
                               boost::asio::thread_pool& pool) {
    auto f_hosts = submit(pool, [&] {
        return ssh_hosts(home_dir() / ".ssh" / "config", active_sesssions);
    });

    auto f_containers = submit(pool, [&] {
        return docker_containers(active_sesssions);
    });

    auto f_sessions = submit(pool, [&] {
            return tmux_paths_and_sessions(active_sesssions);
    });

    return {
        .hosts = f_hosts.get(),
        .containers = f_containers.get(),
        .tmux_sessions = f_sessions.get(),
    };
}

} // namespace allmux
