cmake_minimum_required(VERSION 3.21)
project(DrDirectory
    VERSION 1.0.0
    DESCRIPTION "Fast, good-looking disk usage analyzer with a cushion treemap"
    HOMEPAGE_URL "https://github.com/glenwrhodes/DrDirectory"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup()

set(DRD_SOURCES
    src/main.cpp
    src/FileNode.h src/FileNode.cpp
    src/Roles.h src/Format.h
    src/Theme.h src/Theme.cpp
    src/Icons.h src/Icons.cpp
    src/Splash.h src/Splash.cpp
    src/Platform.h
    src/Scanner.h src/Scanner.cpp
    src/Extensions.h src/Extensions.cpp
    src/DirTreeModel.h src/DirTreeModel.cpp
    src/ExtensionModel.h src/ExtensionModel.cpp
    src/LargestFilesModel.h src/LargestFilesModel.cpp
    src/PercentBarDelegate.h src/PercentBarDelegate.cpp
    src/TreemapWidget.h src/TreemapWidget.cpp
    src/BreadcrumbBar.h src/BreadcrumbBar.cpp
    src/DriveDialog.h src/DriveDialog.cpp
    src/MainWindow.h src/MainWindow.cpp
)

if(WIN32)
    list(APPEND DRD_SOURCES src/Platform_win.cpp resources/DrDirectory.rc)
else()
    list(APPEND DRD_SOURCES src/Platform_unix.cpp)
endif()

if(APPLE)
    set(DRD_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/resources/DrDirectory.icns)
    set_source_files_properties(${DRD_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND DRD_SOURCES ${DRD_ICNS})
endif()

qt_add_executable(DrDirectory WIN32 MACOSX_BUNDLE ${DRD_SOURCES})

qt_add_resources(DrDirectory "appresources"
    PREFIX "/"
    BASE resources
    FILES resources/DrDirectory.ico
)

target_link_libraries(DrDirectory PRIVATE Qt6::Widgets Qt6::Concurrent)
target_compile_definitions(DrDirectory PRIVATE DRD_VERSION="${PROJECT_VERSION}")

set_target_properties(DrDirectory PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.tinymoosh.drdirectory"
    MACOSX_BUNDLE_BUNDLE_NAME "DrDirectory"
    MACOSX_BUNDLE_ICON_FILE "DrDirectory.icns"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Tiny Moosh Games Inc."
)

if(WIN32)
    target_compile_definitions(DrDirectory PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(DrDirectory PRIVATE shell32 ole32 uuid advapi32)
endif()

if(MSVC)
    target_compile_options(DrDirectory PRIVATE /W3 /utf-8 /permissive- /MP)
else()
    target_compile_options(DrDirectory PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS DrDirectory RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/drdirectory.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/drdirectory.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
    install(FILES packaging/linux/com.tinymoosh.drdirectory.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
