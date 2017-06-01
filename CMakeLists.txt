project(NaiveHTTPD)
set(SRC_LIST naivehttpd.c httpdutils.c)
set(CMAKE_C_FLAGS "-Wall -std=c11")
add_executable(NaiveHTTPD ${SRC_LIST})
