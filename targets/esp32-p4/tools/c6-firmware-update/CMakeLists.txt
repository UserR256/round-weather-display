cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PARTITION_TABLE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv")

project(c6_firmware_update)
