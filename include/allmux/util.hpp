#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace allmux {

[[nodiscard]] std::string trim(std::string value);
[[nodiscard]] std::filesystem::path home_dir();
[[nodiscard]] std::filesystem::path config_dir();
[[nodiscard]] bool contains(const std::vector<std::string>& values,
                            std::string_view value);
[[nodiscard]] std::string join_fields(const std::vector<std::string>& fields);

} // namespace allmux
