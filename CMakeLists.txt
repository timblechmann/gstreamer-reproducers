cmake_minimum_required(VERSION 3.5)

project(gstreamer-scratchpad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)

pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)

add_executable(gstreamer-scratchpad main.cpp)

target_link_libraries(gstreamer-scratchpad PUBLIC PkgConfig::gstreamer)

include(GNUInstallDirs)
install(TARGETS gstreamer-scratchpad
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
