# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

# Named VERSION.txt: a file called "version" in an include directory would break
# "#include <version>" on case-insensitive file systems such as Windows.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" ABSAND_VERSION LIMIT_COUNT 1)
string(STRIP "${ABSAND_VERSION}" ABSAND_VERSION)
if(NOT ABSAND_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION.txt must contain a semantic version such as 1.2.3")
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

# ============================================
# FRONTEND SELECTION
# ============================================
# The Kirigami/QML front end needs a KF6 Kirigami that was built for the very
# same Qt installation Absand is compiled against, because it ships QML modules
# inside that Qt's import directory. "auto" checks that before enabling it and
# otherwise keeps the classic widget dialog.
set(ABSAND_FRONTEND "auto" CACHE STRING "Frontend to build: auto, widgets, or kirigami")
set_property(CACHE ABSAND_FRONTEND PROPERTY STRINGS auto widgets kirigami)

set(ABSAND_KIRIGAMI_QML_AVAILABLE OFF)
set(_absand_qml_import_root "")
if(TARGET Qt6::qmake)
    get_target_property(_absand_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_absand_qmake)
        execute_process(COMMAND "${_absand_qmake}" -query QT_INSTALL_QML
                        OUTPUT_VARIABLE _absand_qml_import_root
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
    endif()
endif()
if(_absand_qml_import_root AND EXISTS "${_absand_qml_import_root}/org/kde/kirigami")
    set(ABSAND_KIRIGAMI_QML_AVAILABLE ON)
endif()

set(ABSAND_USE_KIRIGAMI OFF)
if(ABSAND_FRONTEND STREQUAL "kirigami")
    find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)
    find_package(KF6Kirigami REQUIRED)
    if(NOT ABSAND_KIRIGAMI_QML_AVAILABLE)
        message(WARNING "KF6 Kirigami was found, but its QML module is not in "
                        "${_absand_qml_import_root}. Install the Kirigami QML module for this Qt "
                        "installation, or the window will fail to load at runtime.")
    endif()
    set(ABSAND_USE_KIRIGAMI ON)
elseif(ABSAND_FRONTEND STREQUAL "auto")
    find_package(KF6Kirigami QUIET)
    if(KF6Kirigami_FOUND AND ABSAND_KIRIGAMI_QML_AVAILABLE)
        find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)
        set(ABSAND_USE_KIRIGAMI ON)
    endif()
endif()
message(STATUS "Absand frontend: ${ABSAND_FRONTEND} (Kirigami: ${ABSAND_USE_KIRIGAMI}, "
               "QML import root: ${_absand_qml_import_root})")

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
    src/ui/ArchiveBrowserDialog.cpp
)

if(ABSAND_USE_KIRIGAMI)
    # Kirigami/QML front end: the window lives in src/qml and is rendered from
    # the plugin schemas; the archive browser stays a widget dialog.
    list(APPEND CORE_SOURCES
        src/app/AppBackend.cpp
        src/app/ArchiveBrowser.cpp
    )
else()
    list(APPEND CORE_SOURCES
        src/ui/MainDialog.cpp
        src/ui/DestinationSettingsDialog.cpp
        src/ui/SendSettingsDialog.cpp
        src/ui/ProfilePage.cpp
        src/ui/FileDropListWidget.cpp
        src/TestHarness.cpp
        src/TestHarness.h
    )
endif()

set(CORE_HEADERS
    src/models/FilePathListModel.h
    src/models/ArchiveEntryModel.h
    src/core/PluginManager.h
    src/core/CurlExecutable.h
    src/core/ProcessWait.h
    src/core/SendHttpHelper.h
    src/core/SmtpMessage.h
    src/core/PluginConfigForm.h
    src/core/WebShareUpload.h
    src/core/SevenZipRuntime.h
    src/core/UserConfig.h
    src/core/TransferHelpers.h
    src/core/ThemeManager.h
    src/ui/ArchiveBrowserDialog.h
    src/interfaces/ConfigSchema.h
    src/interfaces/PluginInterface.h
    src/interfaces/ArchivePluginInterface.h
    src/interfaces/SendPluginInterface.h
    src/interfaces/DestinationPluginInterface.h
)

if(ABSAND_USE_KIRIGAMI)
    list(APPEND CORE_HEADERS src/app/AppBackend.h src/app/ArchiveBrowser.h)
else()
    list(APPEND CORE_HEADERS
        src/ui/MainDialog.h
        src/ui/DestinationSettingsDialog.h
        src/ui/SendSettingsDialog.h
        src/ui/ProfilePage.h
        src/ui/FileDropListWidget.h
        src/TestHarness.h
    )
endif()

add_executable(Absand
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Without the GUI subsystem Windows starts Absand with a console window next to
# the main window. Diagnostics are written to the log file installed in main.cpp.
# Qt6::EntryPoint supplies the WinMain shim for a plain main() on MinGW.
if(WIN32)
    set_target_properties(Absand PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET Qt6::EntryPoint)
        target_link_libraries(Absand PRIVATE Qt6::EntryPoint)
    endif()
endif()

target_link_libraries(Absand PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${LIBZIP_LIBRARIES}
    bit7z
)

if(ABSAND_USE_KIRIGAMI)
    target_compile_definitions(Absand PRIVATE ABSAND_KIRIGAMI_FRONTEND)
    target_link_libraries(Absand PRIVATE Qt6::Quick Qt6::QuickControls2 KF6::Kirigami)
    # The QML files live in src/qml, so they need a resource alias to end up at
    # the module root (qrc:/qt/qml/Absand/Main.qml) instead of below src/qml/.
    set(ABSAND_QML_FILES
        src/qml/Main.qml
        src/qml/SchemaForm.qml
        src/qml/SettingsPage.qml
        src/qml/ArchiveBrowserPage.qml
    )
    foreach(_absand_qml IN LISTS ABSAND_QML_FILES)
        get_filename_component(_absand_qml_name "${_absand_qml}" NAME)
        set_source_files_properties("${_absand_qml}" PROPERTIES
            QT_RESOURCE_ALIAS "${_absand_qml_name}")
    endforeach()

    # The module output must not share a name with the executable in the build
    # directory, hence the explicit output directory.
    # RESOURCE_PREFIX has to be explicit: without it the module would live at
    # :/Absand instead of :/qt/qml/Absand and the engine would not find it.
    qt_add_qml_module(Absand
        URI Absand
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml-module"
        QML_FILES ${ABSAND_QML_FILES}
    )
endif()

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
    if(ABSAND_USE_KIRIGAMI)
        # The Kirigami QML module is not detected by shlibdeps.
        string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS
               ", qml6-module-org-kde-kirigami, qml6-module-org-kde-desktop")
    endif()
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