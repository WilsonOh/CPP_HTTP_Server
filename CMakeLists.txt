cmake_minimum_required(VERSION 3.23.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project(HttpServer)

function(add_headers VAR)
  set(headers ${${VAR}})
  foreach (header ${ARGN})
    set(headers ${headers} include/${header})
  endforeach()
  set(${VAR} ${headers})
endfunction()

add_headers(HEADERS HttpServer.hpp strutil.hpp get_ip.hpp)

add_library(${PROJECT_NAME} ${HEADERS} src/HttpServer.cpp README.md)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(${PROJECT_NAME} -lfmt)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic)

target_include_directories(${PROJECT_NAME} PUBLIC include src)
