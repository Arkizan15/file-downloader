cmake_minimum_required(VERSION 3.20)
project(file_downloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cari FTXUI di sistem (MSYS2/vcpkg/conan), jika tidak ada download via FetchContent
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    message(STATUS "FTXUI tidak ditemukan di sistem, mengunduh via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
    )
    FetchContent_MakeAvailable(ftxui)
else()
    message(STATUS "FTXUI ditemukan di sistem.")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/downloader.cpp
    src/history.cpp
    src/ui_components.cpp
)

# Header include paths
set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(file-downloader ${SOURCES})

target_include_directories(file-downloader PRIVATE ${INCLUDES})

target_link_libraries(file-downloader PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    wininet
)

if(MSVC)
    target_compile_options(file-downloader PRIVATE /W4 /EHsc /utf-8)
else()
    target_compile_options(file-downloader PRIVATE -Wall -Wextra -Wpedantic)
endif()
