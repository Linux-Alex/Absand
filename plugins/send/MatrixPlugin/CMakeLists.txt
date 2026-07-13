# SPDX-License-Identifier: MPL-2.0

project(MatrixPlugin)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_library(MatrixPlugin SHARED MatrixPlugin.cpp)
target_link_libraries(MatrixPlugin Qt6::Core Qt6::Widgets)
target_include_directories(MatrixPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
set_target_properties(MatrixPlugin PROPERTIES CXX_VISIBILITY_PRESET default LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/send PREFIX "")
set(CMAKE_AUTOMOC ON)
