#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace allmux {

struct SshHost {
    std::string alias;
    std::string hostname;
    std::string user;
    std::optional<std::string> description;
    bool is_active_tmux = false;
};

struct DockerContainer {
    std::string id;
    std::string name;
    std::string image;
    std::string command;
    std::string created_at;
    std::string status_text;
    std::string ports;
    bool status = false;
    bool is_active = false;
};

struct TmuxSession {
    std::optional<std::string> full_path;
    std::string basename;
    bool is_active = false;
};

using EntryData = std::variant<SshHost, DockerContainer, TmuxSession>;

struct Entry {
    EntryData data;
};

struct UiAction {
    enum class Type { ssh, docker, tmux } type;
    std::string name;
    std::optional<std::string> path;
};

struct AppData {
    std::vector<SshHost> hosts;
    std::vector<DockerContainer> containers;
    std::vector<TmuxSession> tmux_sessions;
};

} // namespace allmux
