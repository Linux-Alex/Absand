# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ABSAND_VERSION LIMIT_COUNT 1)
string(STRIP "${ABSAND_VERSION}" ABSAND_VERSION)
if(NOT ABSAND_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version such as 1.2.3")
endif()

project(Absand VERSION "${ABSAND_VERSION}" LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINED_HIDDEN ON)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools)

# Find libzip (with AES-256 support)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED libzip)

# bit7z + 7-Zip source tree for encrypted 7z support
set(BIT7Z_CUSTOM_7ZIP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/7zip" CACHE STRING "Path to the 7-Zip source tree")
set(BIT7Z_BUILD_TESTS OFF CACHE BOOL "Disable bit7z tests" FORCE)
set(BIT7Z_BUILD_DOCS OFF CACHE BOOL "Disable bit7z docs" FORCE)
set(BIT7Z_INSTALL OFF CACHE BOOL "Do not install bit7z development files" FORCE)
add_subdirectory(libs/bit7z)
if(TARGET bit7z)
    set_target_properties(bit7z PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ============================================
# AUTOMATIC PLUGIN DISCOVERY
# ============================================

# Discover and add all plugin subdirectories automatically
file(GLOB_RECURSE ALL_PLUGIN_CMAKE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/CMakeLists.txt")
foreach(PLUGIN_CMAKE ${ALL_PLUGIN_CMAKE_FILES})
    get_filename_component(PLUGIN_DIR ${PLUGIN_CMAKE} DIRECTORY)
    file(RELATIVE_PATH PLUGIN_REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${PLUGIN_DIR})
    message(STATUS "Found plugin: ${PLUGIN_REL_PATH}")
    add_subdirectory(${PLUGIN_DIR})
endforeach()

# ============================================
# CORE APPLICATION
# ============================================

set(CORE_SOURCES
    src/main.cpp
    src/models/FilePathListModel.cpp
    src/models/ArchiveEntryModel.cpp
    src/core/PluginManager.cpp
    src/core/UserConfig.cpp
    src/core/TransferHelpers.cpp
    src/core/ThemeManager.cpp
    src/ui/MainDialog.cpp
    src/ui/ArchiveBrowserDialog.cpp
    src/ui/DestinationSettingsDialog.cpp
    src/ui/SendSettingsDialog.cpp
    src/ui/FileDropListWidget.cpp
    src/TestHarness.cpp
    src/TestHarness.h
)

set(CORE_HEADERS
    src/models/FilePathListModel.h
    src/models/ArchiveEntryModel.h
    src/core/PluginManager.h
    src/core/SevenZipRuntime.h
    src/core/UserConfig.h
    src/core/TransferHelpers.h
    src/core/ThemeManager.h
    src/ui/MainDialog.h
    src/ui/ArchiveBrowserDialog.h
    src/ui/DestinationSettingsDialog.h
    src/ui/SendSettingsDialog.h
    src/ui/FileDropListWidget.h
    src/TestHarness.h
    src/interfaces/PluginInterface.h
    src/interfaces/ArchivePluginInterface.h
    src/interfaces/SendPluginInterface.h
    src/interfaces/DestinationPluginInterface.h
)

add_executable(Absand
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_link_libraries(Absand PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${LIBZIP_LIBRARIES}
    bit7z
)

target_include_directories(Absand PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBZIP_INCLUDE_DIRS}
)

target_compile_options(Absand PRIVATE
    ${LIBZIP_CFLAGS_OTHER}
)

target_compile_definitions(Absand PRIVATE ABSAND_VERSION="${PROJECT_VERSION}")

# Compile translations and place them beside the application for both Windows and Linux.
set(TRANSLATION_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/absand_en_US.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/absand_sl_SI.ts"
)
set(TRANSLATION_QM_FILES)
foreach(TS_FILE IN LISTS TRANSLATION_TS_FILES)
    get_filename_component(TS_NAME "${TS_FILE}" NAME_WE)
    set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/translations/${TS_NAME}.qm")
    add_custom_command(OUTPUT "${QM_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/translations"
        COMMAND Qt6::lrelease "${TS_FILE}" -qm "${QM_FILE}"
        DEPENDS "${TS_FILE}"
        VERBATIM)
    list(APPEND TRANSLATION_QM_FILES "${QM_FILE}")
endforeach()
add_custom_target(absand_translations ALL DEPENDS ${TRANSLATION_QM_FILES})
add_dependencies(Absand absand_translations)
add_custom_command(TARGET Absand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Absand>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TRANSLATION_QM_FILES} "$<TARGET_FILE_DIR:Absand>/translations")

find_program(SEVENZIP_MAKE_EXECUTABLE NAMES gmake make mingw32-make)
set(SEVENZIP_FORMAT7ZF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/7zip/CPP/7zip/Bundles/Format7zF")
if(SEVENZIP_MAKE_EXECUTABLE)
    if(WIN32)
        set(SEVENZIP_RUNTIME_NAME "7z.dll")
    else()
        set(SEVENZIP_RUNTIME_NAME "7z.so")
    endif()

    set(SEVENZIP_RUNTIME_SOURCE "${SEVENZIP_FORMAT7ZF_DIR}/_o/${SEVENZIP_RUNTIME_NAME}")
    set(SEVENZIP_RUNTIME_OUTPUT "${CMAKE_BINARY_DIR}/${SEVENZIP_RUNTIME_NAME}")

    set(SEVENZIP_MAKE_OPTIONS)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC 16 reports a false-positive array-bounds warning in 7-Zip 26.02.
        # Keep upstream warnings-as-errors enabled except for that diagnostic.
        list(APPEND SEVENZIP_MAKE_OPTIONS "CXX_WARN_FLAGS=-Wno-error=array-bounds")
    endif()

    add_custom_target(sevenzip_runtime ALL
        COMMAND ${SEVENZIP_MAKE_EXECUTABLE} -C "${SEVENZIP_FORMAT7ZF_DIR}" -f makefile.gcc ${SEVENZIP_MAKE_OPTIONS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SEVENZIP_RUNTIME_SOURCE}" "${SEVENZIP_RUNTIME_OUTPUT}"
        BYPRODUCTS "${SEVENZIP_RUNTIME_SOURCE}" "${SEVENZIP_RUNTIME_OUTPUT}"
        COMMENT "Building bundled 7-Zip runtime"
        VERBATIM
    )

    add_dependencies(Absand sevenzip_runtime)

    add_custom_command(TARGET Absand POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SEVENZIP_RUNTIME_OUTPUT}" $<TARGET_FILE_DIR:Absand>
        COMMENT "Bundling 7-Zip runtime next to the executable"
    )
else()
    message(WARNING "GNU make was not found, so the bundled 7-Zip runtime target was not created.")
endif()

# For bit7z, we need to add its include directory if not already included
if(TARGET bit7z)
    get_target_property(BIT7Z_INCLUDE_DIR bit7z INTERFACE_INCLUDE_DIRECTORIES)
    if(BIT7Z_INCLUDE_DIR)
        target_include_directories(Absand PRIVATE ${BIT7Z_INCLUDE_DIR})
    endif()
endif()

# ============================================
# PLUGIN OUTPUT MANAGEMENT
# ============================================

# Create plugins directory after build
add_custom_command(TARGET Absand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/plugins
    COMMENT "Creating plugins directory"
)

# Keep the project and bundled third-party license texts beside build and
# installed binaries. External packagers must add the exact Qt/curl license
# bundle selected for their release; see docs/RELEASE_LICENSE_CHECKLIST.md.
set(ABSAND_LICENSE_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/license-bundle")
file(MAKE_DIRECTORY "${ABSAND_LICENSE_SOURCE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
               "${ABSAND_LICENSE_SOURCE_DIR}/Absand-MPL-2.0.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
               "${ABSAND_LICENSE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/docs/RELEASE_LICENSE_CHECKLIST.md"
               "${ABSAND_LICENSE_SOURCE_DIR}/RELEASE_LICENSE_CHECKLIST.md" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libs/bit7z/LICENSE"
               "${ABSAND_LICENSE_SOURCE_DIR}/bit7z-MPL-2.0.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libs/QArchive/LICENSE"
               "${ABSAND_LICENSE_SOURCE_DIR}/QArchive-BSD-3-Clause.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libs/7zip/DOC/License.txt"
               "${ABSAND_LICENSE_SOURCE_DIR}/7-Zip-License.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libs/7zip/DOC/copying.txt"
               "${ABSAND_LICENSE_SOURCE_DIR}/7-Zip-LGPL-2.1.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libs/7zip/DOC/unRarLicense.txt"
               "${ABSAND_LICENSE_SOURCE_DIR}/7-Zip-unRAR-Restriction.txt" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/licenses/libzip-BSD-3-Clause.txt"
               "${ABSAND_LICENSE_SOURCE_DIR}/libzip-BSD-3-Clause.txt" COPYONLY)

add_custom_command(TARGET Absand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ABSAND_LICENSE_SOURCE_DIR}" "$<TARGET_FILE_DIR:Absand>/licenses"
    COMMENT "Bundling project and third-party license notices"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ============================================
# INSTALLATION
# ============================================

# Install main executable
install(TARGETS Absand RUNTIME DESTINATION bin)
if(WIN32)
    install(FILES ${TRANSLATION_QM_FILES} DESTINATION bin/translations)
    install(DIRECTORY "${ABSAND_LICENSE_SOURCE_DIR}/" DESTINATION bin/licenses)
else()
    install(FILES ${TRANSLATION_QM_FILES} DESTINATION share/absand/translations)
    install(DIRECTORY "${ABSAND_LICENSE_SOURCE_DIR}/" DESTINATION share/doc/absand/licenses)
endif()

if(SEVENZIP_MAKE_EXECUTABLE)
    if(WIN32)
        install(FILES "${CMAKE_BINARY_DIR}/7z.dll" DESTINATION bin)
    else()
        install(FILES "${CMAKE_BINARY_DIR}/7z.so" DESTINATION bin)
    endif()
endif()

# Install all plugins
install(DIRECTORY ${CMAKE_BINARY_DIR}/plugins/
    DESTINATION bin/plugins
    FILES_MATCHING PATTERN "*.so" PATTERN "*.dll" PATTERN "*.dylib"
    PATTERN "*.a" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "*_autogen" EXCLUDE
    PATTERN "*Plugin" EXCLUDE
)

# ============================================
# PLATFORM-SPECIFIC CONTEXT MENU INTEGRATION
# ============================================

# The Windows installer owns Explorer registration so packaging never mutates
# the build machine's registry. See packaging/windows/Absand.iss.

# macOS: Add to Finder context menu via Services
if(APPLE)
    set(MACOS_SERVICE_PLIST "${CMAKE_BINARY_DIR}/Absand.service.plist")
    file(WRITE "${MACOS_SERVICE_PLIST}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "<plist version=\"1.0\">\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "<dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "    <key>NSServices</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "    <array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "        <dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSMenuItem</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "                <key>default</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "                <string>Send with Absand</string>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            </dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSSendFileTypes</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "                <string>public.item</string>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            </array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSRequiredContext</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <dict/>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSPortName</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <string>Absand</string>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSMessageName</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <string>openFiles</string>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <key>NSSendTypes</key>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            <array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "                <string>public.file-url</string>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "            </array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "        </dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "    </array>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "</dict>\n")
    file(APPEND "${MACOS_SERVICE_PLIST}" "</plist>\n")

    install(FILES "${MACOS_SERVICE_PLIST}"
        DESTINATION "Contents/Library/LoginItems"
    )

    install(CODE "
        execute_process(COMMAND /System/Library/CoreServices/pbs -flush)
        message(STATUS \"macOS service registered. Restart Finder to see context menu.\")
    ")
endif()

# Linux/KDE: Dolphin Service Menu
if(UNIX AND NOT APPLE)
    set(DESKTOP_FILE "${CMAKE_BINARY_DIR}/absand.desktop")
    set(APP_DESKTOP_FILE "${CMAKE_BINARY_DIR}/Absand.desktop")

    # Create the desktop file directly
    file(WRITE "${DESKTOP_FILE}" "[Desktop Entry]\n")
    file(APPEND "${DESKTOP_FILE}" "Type=Service\n")
    file(APPEND "${DESKTOP_FILE}" "ServiceTypes=KonqPopupMenu/Plugin\n")
    file(APPEND "${DESKTOP_FILE}" "MimeType=all/all;inode/directory;\n")
    file(APPEND "${DESKTOP_FILE}" "Actions=sendWithAbsand;\n")
    file(APPEND "${DESKTOP_FILE}" "X-KDE-Priority=TopLevel\n")
    file(APPEND "${DESKTOP_FILE}" "X-KDE-StartupNotify=true\n\n")
    file(APPEND "${DESKTOP_FILE}" "[Desktop Action sendWithAbsand]\n")
    file(APPEND "${DESKTOP_FILE}" "Name=Send with Absand\n")
    file(APPEND "${DESKTOP_FILE}" "Icon=archive-encrypted\n")
    file(APPEND "${DESKTOP_FILE}" "Exec=Absand %F\n")

    install(FILES "${DESKTOP_FILE}"
        DESTINATION "share/kio/servicemenus"
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/linux/absand-app.desktop.in"
        "${APP_DESKTOP_FILE}"
        @ONLY
    )

    install(FILES "${APP_DESKTOP_FILE}"
        DESTINATION "share/applications"
    )
endif()

# ============================================
# RELEASE PACKAGING
# ============================================

set(CPACK_PACKAGE_NAME "Absand")
set(CPACK_PACKAGE_VENDOR "Linux-Alex")
set(CPACK_PACKAGE_CONTACT "Linux-Alex <alex.marinic22@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Secure archive creation, transfer, and password delivery")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Linux-Alex/Absand")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON)

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Linux-Alex <alex.marinic22@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "curl")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)

# ============================================
# BUILD SUMMARY
# ============================================

add_custom_command(TARGET Absand POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "========================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Absand Build Complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Executable: ${CMAKE_BINARY_DIR}/Absand"
    COMMAND ${CMAKE_COMMAND} -E echo "Plugins will be in: ${CMAKE_BINARY_DIR}/plugins/"
    COMMAND ${CMAKE_COMMAND} -E echo "Bit7z target: $<IF:$<TARGET_EXISTS:bit7z>,available,missing>"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================"
    COMMENT "Build complete"
)
