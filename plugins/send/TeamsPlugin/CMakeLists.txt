# SPDX-License-Identifier: MPL-2.0

project(TeamsPlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(TeamsPlugin SHARED TeamsPlugin.cpp)
target_link_libraries(TeamsPlugin Qt6::Core Qt6::Widgets)
target_include_directories(TeamsPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(TeamsPlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send PREFIX "")
set(CMAKE_AUTOMOC ON)
