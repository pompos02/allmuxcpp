#pragma once

#include <concepts>
#include <format>
#include <optional>
#include <string>
#include <string_view>
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

    // std::string_view get_key() const {
    //     return std::visit( [](const auto& entry) -> std::string_view {
    //             using T = std::decay_t<decltype(entry)>;
    //
    //             if constexpr (std::same_as<T, SshHost>) {
    //                 return entry.alias;
    //             } else if constexpr (std::same_as<T, DockerContainer>) {
    //                 return entry.name;
    //             } else if constexpr (std::same_as<T, TmuxSession>) {
    //                 return entry.basename;
    //             }
    //         },
    //         data);
    // }

    template<class... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    template<class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    std::string_view get_key() const {
        return std::visit(Overloaded{
            [](const SshHost& s) -> std::string_view {return s.alias;},
            [](const DockerContainer& d) -> std::string_view {return d.name;},
            [](const TmuxSession& t) -> std::string_view {return t.basename;},
        }, data);
    }
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
