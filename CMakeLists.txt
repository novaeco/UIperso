cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET "esp32s3")
# Exclude optional test-only components that may be missing from the toolchain bundle
set(EXCLUDE_COMPONENTS catch2)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(UIperso)
