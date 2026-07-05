#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace allmux
{

struct SshHost
{
    std::string                 alias;
    std::string                 hostname;
    std::string                 user;
    std::optional<std::string>  description;
    bool                        is_active_tmux = false;
};

struct DockerContainer
{
    std::string id;
    std::string name;
    std::string image;
    std::string command;
    std::string created_at;
    std::string status_text;
    std::string ports;
    bool        status = false;
    bool        is_active = false;
};

struct TmuxSession
{
    std::optional<std::string> full_path;
    std::string basename;
    bool is_active = false;
};

using EntryData = std::variant<SshHost, DockerContainer, TmuxSession>;

enum class EntryKind
{
    Ssh,
    Docker,
    Tmux,
};

struct EntryKeyVisitor
{
    std::string_view operator()(const SshHost& s) const
    {
        return s.alias;
    }

    std::string_view operator()(const DockerContainer& d) const
    {
        return d.name;
    }

    std::string_view operator()(const TmuxSession& t) const
    {
        return t.basename;
    }
};

struct EntryInfo
{
    EntryKind   kind;
    bool        is_active; 
};


struct Entry
{
    EntryData data;
    
    std::string_view get_key() const
    {
        return std::visit(EntryKeyVisitor{}, data);
    }

    EntryKind kind() const
    {
        return std::visit([](const auto& value) -> EntryKind {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, SshHost>)
                return EntryKind::Ssh;
            else if constexpr (std::is_same_v<T, DockerContainer>)
                return EntryKind::Docker;
            else if constexpr (std::is_same_v<T, TmuxSession>)
                return EntryKind::Tmux;

        }, data);
    }

    EntryInfo info() const
    {
        return std::visit( [](const auto& value) -> EntryInfo {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, SshHost>)
            {
                return {
                    .kind = EntryKind::Ssh,
                    .is_active = value.is_active_tmux,
                };
            }
            else if constexpr (std::is_same_v<T, DockerContainer>)
            {
                return {
                    .kind = EntryKind::Docker,
                    .is_active = value.is_active,
                };
            }
            else if constexpr (std::is_same_v<T, TmuxSession>)
            {
                return {
                    .kind = EntryKind::Tmux,
                    .is_active = value.is_active,
                };
            }
        }, data);
    }
};

struct UiAction
{
    enum class Type
    {
        ssh,
        docker,
        tmux
    } type;
    std::string name;
    std::optional<std::string> path;
};

struct AppData
{
    std::vector<SshHost> hosts;
    std::vector<DockerContainer> containers;
    std::vector<TmuxSession> tmux_sessions;
};

} // namespace allmux
