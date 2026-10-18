# SPDX-License-Identifier: MPL-2.0

project(DiscordPlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(DiscordPlugin SHARED DiscordPlugin.cpp)
target_link_libraries(DiscordPlugin Qt6::Core Qt6::Widgets)
target_include_directories(DiscordPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(DiscordPlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send PREFIX "")
set(CMAKE_AUTOMOC ON)
