cmake_minimum_required(VERSION 3.16)

#configure_file(config-X11.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-X11.h)

set(PROJECT dde-appmenu)

project(${PROJECT})

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

set(CMAKE_CXX_STANDARD 17)

# 启用 qt moc 的支持
set(CMAKE_AUTOMOC ON)
# 启用 qrc 资源文件的支持
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(KF6WindowSystem REQUIRED)


set(SRCS
        menuimporter.h menuimporter.cpp
        dbusmenutypes_p.h dbusmenutypes_p.cpp
        dbusmenushortcut_p.h dbusmenushortcut_p.cpp
        appmenumodel.h appmenumodel.cpp
        dbusmenuimporter.h dbusmenuimporter.cpp
        dbusmenu_interface.h dbusmenu_interface.cpp
        utils_p.h utils.cpp
        menuimporteradaptor.h menuimporteradaptor.cpp
        )

#qt5_add_dbus_adaptor(SRCS com.canonical.AppMenu.Registrar.xml
#    menuimporter.h MenuImporter menuimporteradaptor MenuImporterAdaptor)
#
#qt5_add_dbus_interface(SRCS com.canonical.dbusmenu.xml dbusmenu_interface)


add_library(${PROJECT} ${SRCS})

target_include_directories(${PROJECT} PUBLIC
        ${Qt6Widgets_INCLUDE_DIRS}
        ${Qt6DBus_INCLUDE_DIRS}
        )

target_link_libraries(${PROJECT}
        Qt6::Widgets
        Qt6::DBus
        KF6::WindowSystem
        xcb
        )
