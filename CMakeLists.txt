cmake_minimum_required(VERSION 3.23.2)

project(HttpServer)

add_subdirectory(fmt)

function(add_headers VAR)
  set(headers ${${VAR}})
  foreach (header ${ARGN})
    set(headers ${headers} include/${header})
  endforeach()
  set(${VAR} ${headers})
endfunction()

add_headers(HEADERS HttpServer.hpp strutil.hpp get_ip.hpp)

add_library(${PROJECT_NAME} ${HEADERS} src/HttpServer.cpp README.md)

target_link_libraries(${PROJECT_NAME} fmt::fmt)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic)

target_include_directories(${PROJECT_NAME} PUBLIC include src)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
