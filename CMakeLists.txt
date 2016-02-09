cmake_minimum_required( VERSION 2.6 )
project( uxml )
if( NOT MSVC )
set( CMAKE_C_FLAGS_RELEASE "-O2 -Wall" )
set( CMAKE_C_FLAGS_DEBUG "-g -Wall" )
set( CMAKE_EXE_LINKER_FLAGS_DEBUG "-g" )
endif()

include_directories( ${CMAKE_SOURCE_DIR} )

add_library( uxml uxml.c )

add_executable( test_uxml test_uxml.c )
target_link_libraries( test_uxml uxml )

add_executable( dump_xml dump_xml.c )
target_link_libraries( dump_xml uxml )

add_executable( bench_uxml bench_uxml.c )
target_link_libraries( bench_uxml uxml )

add_executable( test_plusplus test_plusplus.cpp )
target_link_libraries( test_plusplus uxml )
