#include "allmux/parser.hpp"
#include "allmux/tmux.hpp"
#include "allmux/ui.hpp"

#include <exception>
#include <iostream>

// Main function
int main() {
    try {
        auto data = allmux::load_app_data();
        const auto active_sessions = allmux::tmux_sessions();
        const auto action = allmux::run_ui(std::move(data));

        if (!action) {
            return 0;
        }

        switch (action->type) {
        case allmux::UiAction::Type::ssh:
            allmux::launch_ssh_session(action->name, active_sessions);
            break;
        case allmux::UiAction::Type::docker:
            allmux::launch_docker_session(action->name, active_sessions);
            break;
        case allmux::UiAction::Type::tmux:
            allmux::launch_tmux_session(action->name, action->path,
                                        active_sessions);
            break;
        }
    } catch (const std::exception& error) {
        std::cerr << "allmuxcpp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
