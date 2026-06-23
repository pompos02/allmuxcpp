#pragma once

#include "allmux/model.hpp"

#include <optional>

namespace allmux {

[[nodiscard]] std::optional<UiAction> run_ui();

} // namespace allmux
