cmake_minimum_required(VERSION 3.2)
project(vdr_epg_daemon)

add_definitions(-DUSELIBXML)
#add_definitions(-DUSECURL)

include_directories(${MY_SOURCE_DIR}/../)
include_directories(/usr/include/libxml2)
include_directories(/usr/include/mysql)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(SOURCE_FILES
        tvsp.c
        tvsp.h)

set_source_files_properties(${SOURCE_FILES} PROPERTIES LANGUAGE CXX)

add_library(tvsp ${SOURCE_FILES})
