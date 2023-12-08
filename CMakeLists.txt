cmake_minimum_required(VERSION 3.5)
project(graceful-wm C CXX)

set(CMAKE_CXX_STANDARD 17)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-w -g
        -DPACKAGE_NAME="graceful-wm"
        -DPCRE2_CODE_UNIT_WIDTH=8
        -DSN_API_NOT_YET_FROZEN=1
)

find_package(PkgConfig)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PCRE REQUIRED libpcre)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(PANGO REQUIRED pango pangocairo)
pkg_check_modules(XKB REQUIRED xkbcommon xkbcommon-x11)
pkg_check_modules(SN REQUIRED libstartup-notification-1.0)
pkg_check_modules(XCB REQUIRED xcb xcb-aux xcb-shape xcb-xkb xcb-keysyms xcb-xinerama xcb-xrm xcb-cursor)

set(LIBS -lm -lX11 -lXmu -lev)

add_subdirectory(app)
add_subdirectory(example)
