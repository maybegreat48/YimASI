cmake_minimum_required(VERSION 3.20.x)

project(WTSAPI32 VERSION 1001.0.0 DESCRIPTION "YimMenu ASI Loader")

# libs
include(cmake/async-logger.cmake)
include(cmake/minhook.cmake)
include(cmake/gtav-classes.cmake)

# source
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_FILES
    "${SRC_DIR}/**.hpp"
    "${SRC_DIR}/**.cpp"   
    "${SRC_DIR}/*.def"
)

add_library(${PROJECT_NAME} MODULE ${SRC_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

target_precompile_headers(${PROJECT_NAME} PUBLIC "${SRC_DIR}/common.hpp")

message(STATUS "Setting up include directories.")
target_include_directories(${PROJECT_NAME} PRIVATE
    "${SRC_DIR}"
    "${minhook_SOURCE_DIR}/include"    
    "${gtav_classes_SOURCE_DIR}"
)

message(STATUS "Setting up linked libraries")
target_link_libraries(${PROJECT_NAME} PRIVATE AsyncLogger minhook)

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if(MSVC)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} /DEF:${SRC_DIR}/exports.def")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:${PROJECT_NAME}>"
     "C:\\Games\\GTAV" 
  COMMENT "Copying to output directory")