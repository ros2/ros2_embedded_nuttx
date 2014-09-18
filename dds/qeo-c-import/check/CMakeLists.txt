cmake_minimum_required (VERSION 2.6)

project (check C)

include (ExternalProject)
ExternalProject_Add (
    check
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${CMAKE_INSTALL_PREFIX} CFLAGS=-std=c99
)
