# SPDX-License-Identifier: MPL-2.0

project(WebhookPlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(WebhookPlugin SHARED WebhookPlugin.cpp)
target_link_libraries(WebhookPlugin Qt6::Core Qt6::Widgets)
target_include_directories(WebhookPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(WebhookPlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send PREFIX "")
set(CMAKE_AUTOMOC ON)
