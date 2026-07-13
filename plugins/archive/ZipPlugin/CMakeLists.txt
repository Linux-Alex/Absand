# SPDX-License-Identifier: MPL-2.0

project(ZipPlugin)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED libzip)

add_library(ZipPlugin SHARED
    ZipPlugin.cpp
)

target_link_libraries(ZipPlugin
    Qt6::Core
    ${LIBZIP_LIBRARIES}
)

target_include_directories(ZipPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src
    ${LIBZIP_INCLUDE_DIRS}
)

set_target_properties(ZipPlugin PROPERTIES
    CXX_VISIBILITY_PRESET default
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/archive
    PREFIX ""
)

set(CMAKE_AUTOMOC ON)