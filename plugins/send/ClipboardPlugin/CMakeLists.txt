# SPDX-License-Identifier: MPL-2.0

project(ClipboardPlugin)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(ClipboardPlugin SHARED
    ClipboardPlugin.cpp
)

target_link_libraries(ClipboardPlugin
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(ClipboardPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
)

set_target_properties(ClipboardPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)
