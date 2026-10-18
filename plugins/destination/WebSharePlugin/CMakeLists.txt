# SPDX-License-Identifier: MPL-2.0

project(WebSharePlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(WebSharePlugin SHARED WebSharePlugin.cpp)
target_link_libraries(WebSharePlugin Qt6::Core Qt6::Widgets)
target_include_directories(WebSharePlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(WebSharePlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/destination PREFIX "")
set(CMAKE_AUTOMOC ON)
