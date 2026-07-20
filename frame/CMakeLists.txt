cmake_minimum_required(VERSION 3.16)

set(BIN_NAME gxde-top-panel)

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp" "../widgets/*.h" "../widgets/*.cpp" "util/XUtils.h"
        "util/XUtils.cpp" "../appmenu/*.h" "../appmenu/*.cpp" "../widgets/*.ui ../widgets/QOperationWidget.cpp" "../widgets/QOperationWidget.h")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent DBus Svg LinguistTools)
find_package(Dtk6Widget REQUIRED)
find_package(Dtk6Core REQUIRED)
find_package(Dtk6CMake REQUIRED)
find_package(KF6WindowSystem REQUIRED)
find_package(DFrameworkdbusQt6 REQUIRED)
find_package(LayerShellQt REQUIRED)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11)
pkg_check_modules(QGSettings REQUIRED gsettings-qt6)
pkg_check_modules(DtkGUI REQUIRED dtk6gui)

set(TS_FILES
        ${CMAKE_SOURCE_DIR}/translations/gxde-top-panel_zh_CN.ts
        )

set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_BINARY_DIR}/translations)
qt6_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../widgets ${TS_FILES})
qt6_add_translation(QM_FILES ${TS_FILES})

# driver-manager
add_executable(${BIN_NAME} ${SRCS} ${INTERFACES} ${QM_FILES} item/item.qrc frame.qrc)
target_include_directories(${BIN_NAME} PUBLIC
        ${XCB_EWMH_INCLUDE_DIRS}
        /usr/include/libdframeworkdbus-qt6-6.0/
        ${PROJECT_BINARY_DIR}
        ${QGSettings_INCLUDE_DIRS}
        ${DtkGUI_INCLUDE_DIRS}
        ../interfaces
        controller
        dbus
        item
        item/components
        util
        window
        xcb
        )

target_link_libraries(${BIN_NAME} PRIVATE
        ${XCB_EWMH_LIBRARIES}
        DFrameworkdbusQt6::DFrameworkdbusQt6
        Dtk6::Widget
        Dtk6::Core
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::DBus
        ${QGSettings_LIBRARIES}
        ${DtkGUI_LIBRARIES}
        Qt6::Svg
        KF6::WindowSystem
        LayerShellQt::Interface
        xdo
        X11
        )

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "sw_64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "mips64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()
