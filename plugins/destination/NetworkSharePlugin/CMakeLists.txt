# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)
project(NetworkSharePlugin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(NetworkSharePlugin SHARED
    NetworkSharePlugin.cpp
)

target_link_libraries(NetworkSharePlugin
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(NetworkSharePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
)

set_target_properties(NetworkSharePlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)
