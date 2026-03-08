cmake_minimum_required(VERSION 3.20)
project(fangit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# toml11 — vendored headers in third_party/toml11/

set(SOURCES
    src/main.cpp
    src/TrayManager.h
    src/TrayManager.cpp
    src/ConfigManager.h
    src/ConfigManager.cpp
    src/WatcherManager.h
    src/WatcherManager.cpp
    src/GitManager.h
    src/GitManager.cpp
    src/CommitBatcher.h
    src/CommitBatcher.cpp
    src/WorkflowManager.h
    src/WorkflowManager.cpp
    src/NotifyManager.h
    src/NotifyManager.cpp
    src/resources.qrc
)

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(APP_ICON "${CMAKE_SOURCE_DIR}/images/AppIcon.icns")
    set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND SOURCES ${APP_ICON})

    qt_add_executable(fangit MACOSX_BUNDLE ${SOURCES})

    set_target_properties(fangit PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.disuye.fangit"
        MACOSX_BUNDLE_BUNDLE_NAME "fangit"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
else()
    qt_add_executable(fangit ${SOURCES})
endif()

target_include_directories(fangit PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/toml11
)
target_link_libraries(fangit PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
