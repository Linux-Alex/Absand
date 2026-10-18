# SPDX-License-Identifier: MPL-2.0

project(EmailPlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(EmailPlugin SHARED EmailPlugin.cpp)
target_link_libraries(EmailPlugin Qt6::Core Qt6::Widgets)
target_include_directories(EmailPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(EmailPlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send PREFIX "")
set(CMAKE_AUTOMOC ON)
