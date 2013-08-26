cmake_minimum_required( VERSION 2.6 )
project( uxml )
include_directories( ${CMAKE_SOURCE_DIR} )
add_executable( test_uxml uxml.c test_uxml.c )
