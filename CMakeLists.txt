cmake_minimum_required(VERSION 3.25)

project(allmuxcpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v6.1.9
)

FetchContent_Declare(
  rapidfuzz
  GIT_REPOSITORY https://github.com/rapidfuzz/rapidfuzz-cpp.git
  GIT_TAG v3.3.3
)

FetchContent_MakeAvailable(ftxui)
FetchContent_MakeAvailable(rapidfuzz)

find_package(Boost CONFIG REQUIRED)

add_executable(${PROJECT_NAME})

target_sources(allmuxcpp
  PRIVATE
    src/fuzzy.cpp
    src/main.cpp
    src/parser.cpp
    src/process.cpp
    src/tmux.cpp
    src/ui.cpp
    src/util.cpp
)

target_include_directories(allmuxcpp PRIVATE include)

target_link_libraries(allmuxcpp
  PRIVATE
    Boost::headers
    ftxui::screen
    ftxui::dom
    ftxui::component
    rapidfuzz::rapidfuzz
)

target_compile_options(allmuxcpp PRIVATE -Wall -Wextra -Wpedantic)
