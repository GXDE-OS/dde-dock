
set(PLUGIN_NAME "network")

project(${PLUGIN_NAME})

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp" "../../widgets/*.h" "../../widgets/*.cpp" "../../frame/util/utils.h")

find_package(PkgConfig REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(DtkWidget REQUIRED)

pkg_check_modules(DDE-Network-Utils REQUIRED gxde-network-utils)
pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} network.qrc)
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../system-trays)
target_include_directories(${PLUGIN_NAME} PUBLIC ${DtkWidget_INCLUDE_DIRS}
                                                 ${Qt5DBus_INCLUDE_DIRS}
                                                 ${DFrameworkDBus_INCLUDE_DIRS}
                                                 ${DDE-Network-Utils_INCLUDE_DIRS}
                                                 ../../interfaces
                                                 ../../frame)
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Svg_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${DDE-Network-Utils_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins/system-trays)
