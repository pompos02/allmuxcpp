#include "allmux/ui.hpp"

#include "allmux/fuzzy.hpp"
#include "allmux/tmux.hpp"
#include "allmux/util.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace allmux {
namespace {
using namespace ftxui;

struct Match {
    std::size_t index = 0;
    int score = 0;
};

struct App {
    std::vector<Entry> entries;
    std::string query;
    std::size_t selected = 0;
    bool preview_expanded = false;
    std::optional<std::string> status;
    Color status_color = Color::Default;

    [[nodiscard]] std::vector<Match> filtered_matches() const;
};

[[nodiscard]] std::string docker_status_label(const DockerContainer& container) {
    return container.status ? "running" : "stopped";
}

[[nodiscard]] std::string tmux_display_text(const TmuxSession& session) {
    if (session.is_active) {
        return session.session_name;
    }
    return session.full_path.value_or(session.session_name);
}

[[nodiscard]] bool is_active_tmux(const Entry& entry) {
    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        return host->is_active_tmux;
    }
    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        return container->is_active_tmux;
    }
    return std::get<TmuxSession>(entry.data).is_active;
}

[[nodiscard]] int type_rank(const Entry& entry) {
    if (std::holds_alternative<TmuxSession>(entry.data)) {
        return 3;
    }
    if (std::holds_alternative<SshHost>(entry.data)) {
        return 2;
    }
    return 1;
}

[[nodiscard]] std::vector<std::string> search_fields(const Entry& entry) {
    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        return {host->alias, host->hostname, host->user,
                host->description.value_or(""), "ssh"};
    }
    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        return {container->name, docker_status_label(*container),
                container->status_text, container->id, container->image,
                container->command, container->created_at, container->ports,
                "docker", "doc"};
    }
    const auto& session = std::get<TmuxSession>(entry.data);
    return {tmux_display_text(session), session.session_name,
            session.full_path.value_or(""), "tmux", "mux"};
}

std::vector<Match> App::filtered_matches() const {
    std::vector<Match> matches;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto match = fuzzy_match(join_fields(search_fields(entries[i])), query);
        if (!match.matched) {
            continue;
        }
        matches.push_back({.index = i, .score = match.score});
    }

    std::ranges::sort(matches, [&](const Match& left, const Match& right) {
        const auto& left_entry = entries[left.index];
        const auto& right_entry = entries[right.index];
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (is_active_tmux(left_entry) != is_active_tmux(right_entry)) {
            return is_active_tmux(left_entry) > is_active_tmux(right_entry);
        }
        if (type_rank(left_entry) != type_rank(right_entry)) {
            return type_rank(left_entry) > type_rank(right_entry);
        }
        return left.index < right.index;
    });
    return matches;
}

[[nodiscard]] Decorator selected_style(bool selected) {
    return selected ? bgcolor(Color::GrayDark) | bold : nothing;
}

[[nodiscard]] Element entry_line(const Entry& entry, bool selected) {
    const std::string active = is_active_tmux(entry) ? "*" : "";
    const auto marker = selected ? "▌ " : "  ";

    auto add_active = [&](Elements& line) {
        if (!active.empty()) {
            line.push_back(text(active) | color(Color::Green) | bold |
                           selected_style(selected));
        }
    };

    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        Elements line = {text(marker) | color(Color::Cyan) | selected_style(selected),
                          text(" ") | color(Color::Cyan) | bold |
                              selected_style(selected),
                         text(host->alias) | color(Color::White) |
                             selected_style(selected)};
        add_active(line);
        line.push_back(text("  ") | selected_style(selected));
        line.push_back(text(host->hostname) | color(Color::GrayDark) |
                       selected_style(selected));
        return hbox(std::move(line));
    }

    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        const auto status_color = container->status ? Color::Green : Color::Red;
        Elements line = {text(marker) | color(Color::Blue) | selected_style(selected),
                          text(" ") | color(Color::Blue) | bold |
                              selected_style(selected),
                         text(container->name) | color(Color::White) |
                             selected_style(selected)};
        add_active(line);
        line.push_back(text("  ") | selected_style(selected));
        line.push_back(text(docker_status_label(*container)) | color(status_color) |
                       selected_style(selected));
        return hbox(std::move(line));
    }

    const auto& session = std::get<TmuxSession>(entry.data);
    Elements line = {text(marker) | color(Color::Green) | selected_style(selected),
                      text(" ") | color(Color::Green) | bold |
                          selected_style(selected),
                     text(tmux_display_text(session)) | color(Color::White) |
                         selected_style(selected)};
    add_active(line);
    return hbox(std::move(line));
}

[[nodiscard]] Element search_box(const App& app) {
    Elements line = {text("Search ") | color(Color::Cyan) | bold,
                     text(app.query),
                     text(" ") | color(Color::Black) | bgcolor(Color::Cyan)};
    if (app.status) {
        line.push_back(text("  "));
        line.push_back(text(*app.status) | color(app.status_color) | bold);
    }
    return hbox(std::move(line));
}

[[nodiscard]] std::string value_or_dash(const std::string& value) {
    return value.empty() ? "-" : value;
}

[[nodiscard]] Elements preview_lines(const Entry& entry) {
    Elements lines;
    auto field = [&](const std::string& label, const std::string& value,
                     Color value_color = Color::Default) {
        lines.push_back(hbox({text(label) | color(Color::Blue) | bold,
                              text(" "), text(value) | color(value_color)}));
    };

    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        lines.push_back(text("SSH Host") | color(Color::Cyan) | bold);
        lines.push_back(separatorEmpty());
        field("Host:", value_or_dash(host->alias));
        field("User:", value_or_dash(host->user));
        field("Hostname:", value_or_dash(host->hostname));
        lines.push_back(separatorEmpty());
        lines.push_back(text("Description:") | color(Color::Magenta) | bold);
        std::istringstream desc(host->description.value_or("-"));
        for (std::string line; std::getline(desc, line);) {
            lines.push_back(text(line) | color(Color::Green));
        }
        return lines;
    }

    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        lines.push_back(text("Docker Container") | color(Color::Magenta) | bold);
        lines.push_back(separatorEmpty());
        field("Name", container->name, Color::Yellow);
        field("ID", container->id, Color::Cyan);
        field("Image", container->image, Color::Green);
        field("Command", value_or_dash(container->command));
        lines.push_back(separatorEmpty());
        field("Created", value_or_dash(container->created_at), Color::Blue);
        field("Ports", value_or_dash(container->ports), Color::Magenta);
        field("Status", docker_status_label(*container),
              container->status ? Color::Green : Color::Red);
        field("Details", value_or_dash(container->status_text));
        return lines;
    }

    const auto& session = std::get<TmuxSession>(entry.data);
    lines.push_back(text("Tmux Session") | color(Color::Cyan) | bold);
    lines.push_back(separatorEmpty());
    field("Name:", session.session_name, Color::Green);
    field("Path:", session.full_path.value_or("-"));
    lines.push_back(separatorEmpty());
    lines.push_back(text("Files") | color(Color::Magenta) | bold);
    if (session.preview && !trim(*session.preview).empty()) {
        std::istringstream preview(*session.preview);
        std::size_t count = 0;
        for (std::string line; count < 30 && std::getline(preview, line); ++count) {
            lines.push_back(text(line));
        }
    } else {
        lines.push_back(text("No preview available."));
    }
    return lines;
}

[[nodiscard]] std::optional<UiAction> selected_action(const App& app) {
    const auto matches = app.filtered_matches();
    if (matches.empty() || app.selected >= matches.size()) {
        return std::nullopt;
    }

    const auto& entry = app.entries[matches[app.selected].index];
    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        return UiAction{.type = UiAction::Type::ssh,
                        .name = host->alias,
                        .path = std::nullopt};
    }
    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        return UiAction{.type = UiAction::Type::docker,
                        .name = container->name,
                        .path = std::nullopt};
    }
    const auto& session = std::get<TmuxSession>(entry.data);
    return UiAction{.type = UiAction::Type::tmux,
                    .name = session.session_name,
                    .path = session.full_path};
}

[[nodiscard]] std::optional<std::string> selected_ssh_hostname(const App& app) {
    const auto matches = app.filtered_matches();
    if (matches.empty() || app.selected >= matches.size()) {
        return std::nullopt;
    }

    const auto& entry = app.entries[matches[app.selected].index];
    if (const auto* host = std::get_if<SshHost>(&entry.data);
        host != nullptr && !host->hostname.empty()) {
        return host->hostname;
    }
    return std::nullopt;
}

void delete_previous_word(std::string& query) {
    while (!query.empty() && std::isspace(static_cast<unsigned char>(query.back()))) {
        query.pop_back();
    }
    while (!query.empty() && !std::isspace(static_cast<unsigned char>(query.back()))) {
        query.pop_back();
    }
}

[[nodiscard]] App make_app(AppData data) {
    App app;
    for (auto& host : data.hosts) {
        app.entries.push_back({.data = std::move(host)});
    }
    for (auto& container : data.containers) {
        app.entries.push_back({.data = std::move(container)});
    }
    for (auto& session : data.tmux_sessions) {
        app.entries.push_back({.data = std::move(session)});
    }
    return app;
}

} // namespace

std::optional<UiAction> run_ui(AppData data) {
    auto app = make_app(std::move(data));
    auto screen = ScreenInteractive::Fullscreen();
    std::optional<UiAction> action;

    auto renderer = Renderer([&] {
        const auto matches = app.filtered_matches();
        Elements visible;
        for (std::size_t i = 0; i < matches.size(); ++i) {
            const auto& match = matches[i];
            auto line = entry_line(app.entries[match.index], i == app.selected);
            if (i == app.selected) {
                line = line | focus;
            }
            visible.push_back(line);
        }
        if (visible.empty()) {
            visible.push_back(text("No entries match the current search.") |
                              color(Color::GrayDark));
        }

        auto left = vbox({search_box(app), separator(),
                          vbox(std::move(visible)) | yframe | flex}) |
                    border;

        if (!app.preview_expanded) {
            return left;
        }

        Elements preview;
        if (!matches.empty()) {
            preview = preview_lines(app.entries[matches[app.selected].index]);
        } else {
            preview.push_back(text("No entries match the current search.") |
                              color(Color::GrayDark));
        }
        return hbox({left | flex_grow,
                     vbox(std::move(preview)) | border | flex_grow});
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        const auto matches = app.filtered_matches();
        const auto quit = [&] {
            screen.ExitLoopClosure()();
            return true;
        };

        if (event == Event::Escape || event == Event::CtrlC ||
            (event == Event::Character("q") && app.query.empty())) {
            return quit();
        }
        if (event == Event::Return) {
            action = selected_action(app);
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::ArrowUp || event == Event::CtrlK) {
            if (app.selected > 0) {
                --app.selected;
            }
            return true;
        }
        if (event == Event::ArrowDown || event == Event::CtrlJ) {
            if (app.selected + 1 < matches.size()) {
                ++app.selected;
            }
            return true;
        }
        if (event == Event::PageUp) {
            app.selected = app.selected > 5 ? app.selected - 5 : 0;
            return true;
        }
        if (event == Event::PageDown) {
            app.selected = std::min(app.selected + 5,
                                    matches.empty() ? 0 : matches.size() - 1);
            return true;
        }
        if (event == Event::Backspace) {
            if (!app.query.empty()) {
                app.query.pop_back();
            }
            app.status.reset();
            return true;
        }
        if (event == Event::CtrlU) {
            app.query.clear();
            app.status.reset();
            return true;
        }
        if (event == Event::CtrlW) {
            delete_previous_word(app.query);
            app.status.reset();
            return true;
        }
        if (event == Event::CtrlS) {
            app.preview_expanded = !app.preview_expanded;
            app.status.reset();
            return true;
        }
        if (event == Event::CtrlY) {
            if (const auto hostname = selected_ssh_hostname(app)) {
                if (copy_to_tmux_clipboard(*hostname)) {
                    app.status = "Copied hostname: " + *hostname;
                    app.status_color = Color::Green;
                } else {
                    app.status = "Failed to copy hostname";
                    app.status_color = Color::Red;
                }
            } else {
                app.status = "Ctrl-Y only copies SSH entries with a hostname";
                app.status_color = Color::Yellow;
            }
            return true;
        }
        if (event.is_character()) {
            app.query += event.character();
            app.selected = 0;
            app.status.reset();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace allmux
