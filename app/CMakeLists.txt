project(app C CXX)

include(${CMAKE_SOURCE_DIR}/common/common.cmake)

file(GLOB APP_MAIN *.h *.c)

add_executable(graceful-wm main.c
        ${COMMON_SRC} ${APP_MAIN})

target_link_libraries(graceful-wm
        ${LIBS}
        ${SN_LIBRARIES}
        ${XCB_LIBRARIES}
        ${XKB_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${PCRE_LIBRARIES}
        ${CAIRO_LIBRARIES}
        ${PANGO_LIBRARIES}
)
target_include_directories(graceful-wm PUBLIC
        ${SN_INCLUDE_DIRS}
        ${XCB_INCLUDE_DIRS}
        ${XKB_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${PCRE_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        ${PANGO_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/common
)
