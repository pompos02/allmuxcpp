#include "allmux/ui.hpp"

#include "allmux/fuzzy.hpp"
#include "allmux/parser.hpp"
#include "allmux/tmux.hpp"
#include "allmux/util.hpp"
#include "allmux/history.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cstdlib>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cctype>
#include <ftxui/screen/color.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <exception>
#include <filesystem>

inline constexpr std::string_view ALLMUX_THEME_FILE_NAME =  "color_variant";


namespace allmux {
namespace {
using namespace ftxui;

struct Match {
    std::size_t index = 0;
    int score = 0;
    std::vector<std::size_t> matched_indices;
};

struct App {
    std::vector<Entry> entries;
    std::string query;
    std::size_t selected = 0;
    std::optional<std::string> error;
    bool loading = true;
    std::optional<std::string> status;
    Color status_color = Color::Default;
    bool color_variant = true;
    History history = allmux::History::load_history();
};

namespace fs = std::filesystem;

bool is_dark_variant() {
    fs::path color_file = theme_file();

    // Create the theme file if it doesn't exists
    if (!fs::exists(color_file)) {
        std::ofstream f(color_file);
        if (f.is_open()) {
            f << "dark";
        } else {
            throw std::runtime_error("failed to create: " + color_file.string());
        }
    }
    std::ifstream input_file(color_file);
    std::string line{};
    if (input_file.is_open()) {
        std::getline(input_file, line);
    }
    line = trim(line);
    if (line == "light") {
        return false;
    }
    return true;
}

bool toggle_variant() {
    auto path = theme_file();
    const bool is_dark = is_dark_variant();
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening: " + path.string());
    }
    if (is_dark) {
        file << "light";
        return false;
    } else {
        file << "dark";
        return true;
    }
}

[[nodiscard]]
std::string docker_status_label(const DockerContainer& container) {
    return container.status ? "running" : "stopped";
}

[[nodiscard]] std::string tmux_display_text(const TmuxSession& session) {
    if (session.is_active) {
        return session.basename;
    }
    return session.full_path.value_or(session.basename);
}

[[nodiscard]] bool is_active_tmux(const Entry& entry) {
    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        return host->is_active_tmux;
    }
    if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        return container->is_active;
    }
    return std::get<TmuxSession>(entry.data).is_active;
}

[[nodiscard]] int type_rank(const Entry& entry) {
    return std::holds_alternative<TmuxSession>(entry.data) ? 3
           : std::holds_alternative<SshHost>(entry.data)   ? 2
                                                           : 1;
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
    return {tmux_display_text(session), session.basename,
            session.full_path.value_or(""), "tmux", "mux"};
}

[[nodiscard]]
auto filtered_matches(const App& app) -> std::vector<Match> {
    std::vector<Match> matches;
    for (std::size_t i = 0; i < app.entries.size(); ++i) {
        const auto joined = join_fields(search_fields(app.entries[i]));
        std::vector<std::size_t> out_buffer(joined.size());
        const auto match = fuzzy_match(joined, app.query, out_buffer);
        if (!match.matched) {
            continue;
        }
        auto base_score = match.score;
        auto history_score = app.history.score(app.entries[i].get_key());
        matches.push_back({.index = i,
                           .score = static_cast<int>(base_score + ((double)base_score * history_score / 100)),
                           .matched_indices = {match.matched_indices.begin(),
                                               match.matched_indices.end()}});
    }

    std::ranges::sort(matches, [&](const Match& left, const Match& right) {
        const auto& left_entry = app.entries[left.index];
        const auto& right_entry = app.entries[right.index];
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

[[nodiscard]] Decorator selected_style(bool selected, bool is_dark) {
    if (is_dark) {
        return selected ? bgcolor(Color::GrayDark) | bold : nothing;
    }

    return selected ? bgcolor(Color::RGB(246, 246, 246)) | bold : nothing;
}

Element highlighted_score(int score_amount) {
    Color my_color;
    Decorator style;
    if (score_amount < 30)
    {
        my_color =Color::GrayDark;
    }
    else if (score_amount < 70)
    {
        my_color = Color::Yellow;
    }
    else
    {
        my_color = Color::Green;
    }

    return text(std::to_string(score_amount)) | color(my_color);
}

[[nodiscard]] Element highlighted_text(std::string_view value,
                                       std::span<const std::size_t> indices,
                                       std::size_t offset,
                                       Color base_color,
                                       bool selected,
                                       bool is_dark) {
    Elements parts;
    for (std::size_t pos = 0; pos < value.size();) {
        const bool matched = std::ranges::binary_search(indices, offset + pos);
        auto end = pos + 1;
        while (end < value.size() &&
               std::ranges::binary_search(indices, offset + end) == matched) {
            ++end;
        }

        Decorator style;
        if (is_dark) {
            style = matched ? color(Color::RGB(0, 0, 0)) | bgcolor(Color::Cyan)
                : color(base_color);
            style = style | selected_style(selected, is_dark);
        } else {
            style = matched ? color(Color::RGB(255, 255, 255)) | bgcolor(Color::Cyan)
                : color(base_color);
            style = style | selected_style(selected, is_dark);
        }


        if (matched) {
            style = style | bold;
        }
        parts.push_back(text(std::string{value.substr(pos, end - pos)}) |
                        style);
        pos = end;
    }
    return hbox(std::move(parts));
}

[[nodiscard]]
Element entry_line(const Entry& entry,
                   std::span<const std::size_t> matched_indices,
                   int score,
                   bool selected, bool is_dark)
{

    Color accent = Color::Green;
    Color detail_color = Color::GrayDark;
    std::string icon = " ";
    std::string primary;
    std::optional<std::string> detail;

    if (const auto* host = std::get_if<SshHost>(&entry.data)) {
        accent = Color::Cyan;
        icon = " ";
        primary = host->alias;
        detail = host->hostname;
    } else if (const auto* container = std::get_if<DockerContainer>(&entry.data)) {
        accent = Color::Blue;
        icon = " ";
        primary = container->name;
        detail = docker_status_label(*container);
        detail_color = container->status ? Color::Green : Color::Red;
    } else {
        primary = tmux_display_text(std::get<TmuxSession>(entry.data));
    }

    const auto style = selected_style(selected, is_dark);
    Elements line = {text(selected ? "▌ " : "  ") | color(accent) | style,
                     text(icon) | color(accent) | bold | style,
                     highlighted_text(primary, matched_indices, 0, Color::White,
                                      selected, is_dark)};
    if (is_active_tmux(entry)) {
        line.push_back(text("*") | color(Color::Green) | bold | style);
    }
    if (detail) {
        line.push_back(text("  ") | style);
        line.push_back(highlighted_text(*detail, matched_indices,
                                        primary.size() + 1, detail_color,
                                        selected, is_dark));
    }

    line.push_back(filler() | style);
    line.push_back(highlighted_score(score) | style);

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

[[nodiscard]] UiAction action_for(const Entry& entry) {
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
                    .name = session.basename,
                    .path = session.full_path};
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
    auto append = [&](auto& values) {
        for (auto& value : values) {
            app.entries.push_back({.data = std::move(value)});
        }
    };
    append(data.hosts);
    append(data.containers);
    append(data.tmux_sessions);
    app.color_variant = is_dark_variant();
    app.loading = false;
    return app;
}


} // namespace

std::optional<UiAction> run_ui() {
    App app;
    auto screen = ScreenInteractive::Fullscreen();
    std::optional<UiAction> action;

    boost::asio::thread_pool pool{4};

    boost::asio::post(pool, [&] {
        try {
            auto active_sessions = tmux_sessions();
            auto data = load_app_data_parallel(active_sessions, pool);

            screen.Post([&, data = std::move(data)]() mutable {
                app = make_app(std::move(data));
                app.selected = 0;
                app.status.reset();
            });
        } catch (const std::exception& error) {
            std::string message = error.what();

            screen.Post([&, message = std::move(message)] {
                app.loading = false;
                app.error = message;
                app.status.reset();
            });
        }
        screen.PostEvent(Event::Custom);
    });


    auto renderer = Renderer([&] {
        const auto matches = filtered_matches(app);
        Elements visible;
        for (std::size_t i = 0; i < matches.size(); ++i) {
            const auto& match = matches[i];
            auto line = entry_line(app.entries[match.index], match.matched_indices, match.score, i == app.selected, app.color_variant);
            if (i == app.selected) {
                line = line | focus;
            }
            visible.push_back(line);
        }
        if (app.loading) {
            visible.push_back(text("Loading entries...") |
                              color(Color::GrayDark));
        } else if (app.error) {
            visible.push_back(text("failed to load entries: " + *app.error) |
                              color(Color::Red));
        } else if (visible.empty()) {
            visible.push_back(text("No entries match the current search.") |
                              color(Color::GrayDark));
        }

        return vbox({search_box(app), separator(),
                     vbox(std::move(visible)) | yframe | flex}) |
               border;
    });

    // Keymaps
    auto component = CatchEvent(renderer, [&](Event event) {
        const std::vector<Match> matches = filtered_matches(app);
        const Entry* selected_entry = nullptr;
        if (!matches.empty() && app.selected < matches.size()) {
            selected_entry = &app.entries[matches[app.selected].index];
        }
        const auto quit = [&] {
            screen.ExitLoopClosure()();
            return true;
        };

        if (event == Event::Escape || event == Event::CtrlC ||
            (event == Event::Character("q") && app.query.empty())) {
            return quit();
        }
        if (event == Event::Return) {
            if (app.loading || app.error || selected_entry == nullptr) {
                return true;
            }
            app.history.record_access(selected_entry->get_key());
            action = action_for(*selected_entry);
            return quit();
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
        if (event == Event::CtrlT) {
            app.color_variant = toggle_variant();
        }
        if (event == Event::CtrlY) {
            const auto* host = selected_entry != nullptr
                                   ? std::get_if<SshHost>(&selected_entry->data)
                                   : nullptr;
            if (host != nullptr && !host->hostname.empty()) {
                if (copy_to_tmux_clipboard(host->hostname)) {
                    app.status = "Copied hostname: " + host->hostname;
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
        if (event == Event::Backspace || event == Event::CtrlU ||
            event == Event::CtrlW || event.is_character()) {
            if (event == Event::Backspace && !app.query.empty()) {
                app.query.pop_back();
            } else if (event == Event::CtrlU) {
                app.query.clear();
            } else if (event == Event::CtrlW) {
                delete_previous_word(app.query);
            } else if (event.is_character()) {
                app.query += event.character();
                app.selected = 0;
            }
            app.status.reset();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace allmux
