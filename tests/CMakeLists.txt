# SPDX-License-Identifier: MPL-2.0

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
pkg_check_modules(TEST_LIBARCHIVE REQUIRED libarchive)

add_executable(AbsandTests
    PluginIntegrationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/PluginManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/PluginManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/UserConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/UserConfig.h
)

target_include_directories(AbsandTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(AbsandTests PRIVATE Qt6::Core Qt6::Widgets Qt6::Test bit7z ${LIBZIP_LIBRARIES} ${TEST_LIBARCHIVE_LIBRARIES})
target_include_directories(AbsandTests PRIVATE ${LIBZIP_INCLUDE_DIRS} ${TEST_LIBARCHIVE_INCLUDE_DIRS})
target_compile_options(AbsandTests PRIVATE ${LIBZIP_CFLAGS_OTHER})

# PluginManager searches beside the running executable.
set_target_properties(AbsandTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_dependencies(AbsandTests ZipPlugin SevenZipPlugin ClipboardPlugin MatrixPlugin TeamsPlugin TelegramPlugin LocalFolderPlugin NetworkSharePlugin FTPDestinationPlugin SFTPDestinationPlugin)

add_test(NAME Absand.PluginIntegration COMMAND AbsandTests)
set_tests_properties(Absand.PluginIntegration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
