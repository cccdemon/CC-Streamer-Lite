cmake_minimum_required(VERSION 3.24)

project(CCStreamer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Vulkan)

qt_standard_project_setup()

qt_add_executable(CCStreamer
    src/main.cpp
    src/app/Application.cpp
    src/app/Application.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/config/Profile.cpp
    src/config/Profile.h
    src/logging/Logger.cpp
    src/logging/Logger.h
)

target_include_directories(CCStreamer
    PRIVATE
        src
)

target_link_libraries(CCStreamer
    PRIVATE
        Qt6::Widgets
)

if(Vulkan_FOUND)
    target_compile_definitions(CCStreamer PRIVATE CCSTREAMER_HAS_VULKAN=1)
    target_link_libraries(CCStreamer PRIVATE Vulkan::Vulkan)
else()
    target_compile_definitions(CCStreamer PRIVATE CCSTREAMER_HAS_VULKAN=0)
endif()

if(MSVC)
    target_compile_options(CCStreamer PRIVATE /W4 /permissive-)
else()
    target_compile_options(CCStreamer PRIVATE -Wall -Wextra -Wpedantic)
endif()

