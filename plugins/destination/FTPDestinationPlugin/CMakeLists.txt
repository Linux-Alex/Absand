# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)
project(FTPDestinationPlugin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(FTPDestinationPlugin SHARED
    FTPDestinationPlugin.cpp
    ../../../src/core/TransferHelpers.cpp
)

target_link_libraries(FTPDestinationPlugin
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(FTPDestinationPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
)

set_target_properties(FTPDestinationPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)
