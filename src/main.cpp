#include "allmux/tmux.hpp"
#include "allmux/ui.hpp"

#include <exception>
#include <iostream>

// Main function
int main()
{
    try
    {
        const auto action = allmux::run_ui();
        if (!action)
        {
            return 0;
        }

        const auto active_sessions = allmux::tmux_sessions();

        switch (action->type)
        {
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
    }
    catch (const std::exception& error)
    {
        std::cerr << "allmuxcpp: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
