cmake_minimum_required(VERSION 3.5)

project(dsoreloader LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dsoreloader STATIC
  dsoreloader.cpp
  dsoreloader.h
  shared_lock_guard.h
)

target_compile_definitions(dsoreloader PRIVATE DSORELOADER_LIBRARY)
