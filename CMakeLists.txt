cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED VERSION)
    set(VERSION 4.0)
endif()

project(gxde-top-panel)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_FLAGS "-g -Wall")

# generate a compile commands file as complete database for vim-YouCompleteMe or some other similar tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
endif ()

# Generate .qm files from .ts files
find_program(QT_LRELEASE_EXECUTABLE /usr/lib/qt6/bin/lrelease)
file(GLOB TS_FILES "${CMAKE_SOURCE_DIR}/translations/*.ts")
set(QM_FILES)
foreach(TS_FILE ${TS_FILES})
    get_filename_component(QM_FILE ${TS_FILE} NAME_WE)
    set(QM_FILE "${CMAKE_BINARY_DIR}/translations/${QM_FILE}.qm")
    add_custom_command(
        OUTPUT ${QM_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/translations
        COMMAND ${QT_LRELEASE_EXECUTABLE} ${TS_FILE} -qm ${QM_FILE}
        DEPENDS ${TS_FILE}
        COMMENT "Generating ${QM_FILE}"
    )
    list(APPEND QM_FILES ${QM_FILE})
endforeach()
add_custom_target(translations ALL DEPENDS ${QM_FILES})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DQT_DEBUG")

# Test architecture
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "sw_64")
    # add compiler flags -mieee for mathmatic
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mieee")
    add_definitions(-DDISABLE_SHOW_ANIMATION)
endif ()

file(GLOB INTERFACES "interfaces/*.h")

add_subdirectory("frame")
#add_subdirectory("plugins")
add_subdirectory("appmenu")

# Install interface headers for development library
install(FILES ${INTERFACES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gxde-top-panel)

# Generate and install pkgconfig file
configure_file(
    ${CMAKE_SOURCE_DIR}/gxde-top-panel.pc.in
    ${CMAKE_BINARY_DIR}/gxde-top-panel.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/gxde-top-panel.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Generate and install CMake config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/GxdeTopPanelConfig.cmake.in
    ${CMAKE_BINARY_DIR}/GxdeTopPanelConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GxdeTopPanel
)
install(FILES ${CMAKE_BINARY_DIR}/GxdeTopPanelConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GxdeTopPanel)

if (CMAKE_BUILD_TYPE STREQUAL "RELEASE")
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif (CMAKE_BUILD_TYPE STREQUAL "RELEASE")

execute_process(COMMAND apt --version OUTPUT_VARIABLE APTVERSION RESULT_VARIABLE EXIT_CODE)
if (${EXIT_CODE} EQUAL 0)
    message(STATUS "apt ${APTVERSION} - has found： CPank module enable")
    include(deb.cmake)
    include(CPack)
else()
    message(STATUS "apt not found! - CPank module disable")
    install(FILES ${CMAKE_BINARY_DIR}/frame/dde-top-panel DESTINATION ${CMAKE_INSTALL_BINDIR} 
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES ${CMAKE_SOURCE_DIR}/xml/com.gxde.dde.toppanel.gschema.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas)
    install(FILES ${CMAKE_SOURCE_DIR}/dde-top-panel.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/translations DESTINATION ${CMAKE_INSTALL_DATADIR}/gxde-top-panel)
endif()
