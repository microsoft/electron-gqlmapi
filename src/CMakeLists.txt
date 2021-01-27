# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.17.1)
project(electron-gqlmapi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

# Fetch the https://github.com/microsoft/gqlmapi repository.
include(FetchContent)
FetchContent_Declare(gqlmapi
  GIT_REPOSITORY "https://github.com/microsoft/gqlmapi"
  GIT_TAG "8ad3281e102b2eff87355e45e03eaee04d3fcb5a"
  GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(gqlmapi)

execute_process(COMMAND node -e "require('nan')"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE NAN_INCLUDE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

add_library(${PROJECT_NAME} SHARED
  NodeBinding.cpp
  ${CMAKE_JS_SRC})

target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_NAME=${PROJECT_NAME} BUILDING_NODE_EXTENSION V8_REVERSE_JSARGS)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${CMAKE_CURRENT_SOURCE_DIR}/${NAN_INCLUDE})
target_link_libraries(${PROJECT_NAME} PRIVATE gqlmapi ${CMAKE_JS_LIB})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")