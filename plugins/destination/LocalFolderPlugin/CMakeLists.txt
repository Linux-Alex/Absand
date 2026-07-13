# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)
project(LocalFolderPlugin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(LocalFolderPlugin SHARED
    LocalFolderPlugin.cpp
    ../../../src/core/TransferHelpers.cpp
)

target_link_libraries(LocalFolderPlugin
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(LocalFolderPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
)

set_target_properties(LocalFolderPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)
