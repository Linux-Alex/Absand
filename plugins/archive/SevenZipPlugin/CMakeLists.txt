# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)
project(SevenZipPlugin)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(SevenZipPlugin SHARED
    SevenZipPlugin.cpp
)

target_link_libraries(SevenZipPlugin
    Qt6::Core
    bit7z
)

target_include_directories(SevenZipPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
)

set_target_properties(SevenZipPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/archive
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)
