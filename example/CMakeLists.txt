project(example)

add_executable(tiny-wm-x tiny-wm-x.c)
target_link_libraries(tiny-wm-x -lX11)