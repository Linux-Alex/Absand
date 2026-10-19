# SPDX-License-Identifier: MPL-2.0

# One plugin library per web file host, so every host appears as its own entry
# in the destination list and keeps its own profiles. They all share one
# implementation; ABSAND_WEB_PROVIDER selects the host.
project(WebSharePlugins)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(ABSAND_WEB_PROVIDERS
    gofile
    litterbox
    tempsh
    x0at
    catbox
    tmpfiles
    pixeldrain
    custom
)

foreach(provider IN LISTS ABSAND_WEB_PROVIDERS)
    string(SUBSTRING "${provider}" 0 1 providerFirst)
    string(TOUPPER "${providerFirst}" providerFirst)
    string(SUBSTRING "${provider}" 1 -1 providerRest)
    set(targetName "WebShare${providerFirst}${providerRest}Plugin")

    add_library(${targetName} SHARED WebShareProviderPlugin.cpp)
    target_link_libraries(${targetName} Qt6::Core Qt6::Widgets)
    target_include_directories(${targetName} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../src")
    target_compile_definitions(${targetName} PRIVATE ABSAND_WEB_PROVIDER="${provider}")
    set_target_properties(${targetName} PROPERTIES
        CXX_VISIBILITY_PRESET default
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/destination"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/destination"
        PREFIX ""
    )
endforeach()

set(CMAKE_AUTOMOC ON)
