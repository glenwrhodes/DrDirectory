cmake_minimum_required(VERSION 3.21)
project(DrDirectory VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup()

set(DRD_SOURCES
    src/main.cpp
    src/FileNode.h src/FileNode.cpp
    src/Roles.h src/Format.h
    src/Theme.h src/Theme.cpp
    src/Icons.h src/Icons.cpp
    src/Scanner.h src/Scanner.cpp
    src/Extensions.h src/Extensions.cpp
    src/WinShell.h src/WinShell.cpp
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
    list(APPEND DRD_SOURCES resources/DrDirectory.rc)
endif()

qt_add_executable(DrDirectory WIN32 ${DRD_SOURCES})

qt_add_resources(DrDirectory "appresources"
    PREFIX "/"
    BASE resources
    FILES resources/DrDirectory.ico
)

target_link_libraries(DrDirectory PRIVATE Qt6::Widgets Qt6::Concurrent)

if(WIN32)
    target_compile_definitions(DrDirectory PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(DrDirectory PRIVATE shell32 ole32 uuid advapi32)
endif()
if(MSVC)
    target_compile_options(DrDirectory PRIVATE /W3 /utf-8 /permissive- /MP)
endif()
