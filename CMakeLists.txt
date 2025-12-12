cmake_minimum_required(VERSION 3.21)

project(
  uxs
  DESCRIPTION "Utilities and eXtensionS (UXS) for Standard C++ Library"
  LANGUAGES CXX)

include(ExternalProject)
include(GenerateExportHeader)

option(USE_ZLIB "Use ZLib" ON)
option(USE_LIBZIP "Use LibZip" ON)
option(USE_LIBCPP "Use LibC++" OFF)
option(USE_SANITIZERS_FOR_DEBUG "Use Sanitizers for Debug build" ON)
option(OPTION_EXPORT_COMPILE_DEFS_AND_INCLUDE_DIRS
       "Export compile definitions and include directories" OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Using C++${CMAKE_CXX_STANDARD} standard")

foreach(var_name CMAKE_C_COMPILER CMAKE_C_FLAGS CMAKE_BUILD_TYPE)
  if(DEFINED ${var_name})
    list(APPEND EXT_PROJECT_CMAKE_ARGS "-D${var_name}=${${var_name}}")
  endif()
endforeach()

# ##############################################################################
# Prepare `zlib` library

if((USE_ZLIB OR USE_LIBZIP) AND NOT ZLIB_LIBRARY)
  if(NOT ZLIB_SOURCE_DIR)
    set(ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib.git")
  endif()
  set(ZLIB_LIBRARY_NAME
      $<IF:$<BOOL:${WIN32}>,zlibstatic$<$<CONFIG:Debug>:d>.lib,libz.a>)
  ExternalProject_Add(
    libz
    GIT_REPOSITORY ${ZLIB_GIT_REPOSITORY}
    GIT_TAG 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf
    SOURCE_DIR ${ZLIB_SOURCE_DIR}
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/zlib/lib/${ZLIB_LIBRARY_NAME}
    CMAKE_ARGS -DZLIB_BUILD_EXAMPLES=Off #
               -DSKIP_INSTALL_FILES=On #
               -DCMAKE_POSITION_INDEPENDENT_CODE=On #
               -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> #
               ${EXT_PROJECT_CMAKE_ARGS})
  ExternalProject_Get_Property(libz INSTALL_DIR)
  set(ZLIB_INCLUDE_DIR ${INSTALL_DIR}/include)
  set(ZLIB_LIBRARY ${INSTALL_DIR}/lib/${ZLIB_LIBRARY_NAME})
  set(ZLIB_DEP libz)
endif()

# ##############################################################################
# Prepare `libzip` library

if(USE_LIBZIP AND NOT LIBZIP_LIBRARY)
  if(NOT LIBZIP_SOURCE_DIR)
    set(LIBZIP_GIT_REPOSITORY "https://github.com/nih-at/libzip.git")
  endif()
  set(LIBZIP_LIBRARY_NAME $<IF:$<BOOL:${WIN32}>,zip.lib,libzip.a>)
  ExternalProject_Add(
    libzip
    DEPENDS ${ZLIB_DEP}
    GIT_REPOSITORY ${LIBZIP_GIT_REPOSITORY}
    GIT_TAG a1364389320754b437234c30989e37e1881b4fef
    SOURCE_DIR ${LIBZIP_SOURCE_DIR}
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libzip
    BUILD_BYPRODUCTS
      ${CMAKE_CURRENT_BINARY_DIR}/libzip/lib/${LIBZIP_LIBRARY_NAME}
    CMAKE_ARGS -DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR} #
               -DZLIB_LIBRARY=${ZLIB_LIBRARY} #
               -DENABLE_COMMONCRYPTO=Off #
               -DENABLE_GNUTLS=Off #
               -DENABLE_MBEDTLS=Off #
               -DENABLE_OPENSSL=Off #
               -DENABLE_WINDOWS_CRYPTO=Off #
               -DENABLE_BZIP2=Off #
               -DENABLE_LZMA=Off #
               -DENABLE_ZSTD=Off #
               -DBUILD_TOOLS=Off #
               -DBUILD_REGRESS=Off #
               -DBUILD_OSSFUZZ=Off #
               -DBUILD_EXAMPLES=Off #
               -DBUILD_DOC=Off #
               -DBUILD_SHARED_LIBS=Off #
               -DCMAKE_POSITION_INDEPENDENT_CODE=On #
               -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> #
               -DCMAKE_INSTALL_LIBDIR=lib #
               ${EXT_PROJECT_CMAKE_ARGS})
  ExternalProject_Get_Property(libzip INSTALL_DIR)
  set(LIBZIP_INCLUDE_DIR ${INSTALL_DIR}/include)
  set(LIBZIP_LIBRARY ${INSTALL_DIR}/lib/${LIBZIP_LIBRARY_NAME})
  set(LIBZIP_DEP libzip)
endif()

# ##############################################################################
# Compilation flags

if(MSVC)
  add_compile_options(/Zc:__cplusplus /utf-8 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-nullability-extension)
    if(CMAKE_CXX_STANDARD LESS 17)
      add_compile_options(-Wno-ignored-attributes)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options($<$<CONFIG:Release>:-Wno-array-bounds>)
    add_compile_options($<$<CONFIG:Release>:-Wno-stringop-overflow>)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options($<$<CONFIG:Debug>:-fstandalone-debug>)
    if(USE_LIBCPP)
      add_compile_options(-stdlib=libc++)
      add_link_options(-stdlib=libc++)
    endif()
  endif()
endif()

if(USE_SANITIZERS_FOR_DEBUG)
  message("Using sanitizers for Debug build")
  if(MSVC)
    add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
  else()
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address,undefined,leak>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address,undefined,leak>)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      add_link_options($<$<CONFIG:Debug>:-shared-libsan>)
    endif()
  endif()
endif()

# ##############################################################################
# Add `uxs` build target

if(WIN32)
  set(uxs_platform_dir platform/win)
elseif(UNIX)
  set(uxs_platform_dir platform/posix)
endif()
file(GLOB_RECURSE uxs_headers include/*.h)
file(GLOB_RECURSE uxs_sources src/*.h;src/*.cpp)
file(GLOB_RECURSE uxs_platform_sources
     ${uxs_platform_dir}/src/*.h;${uxs_platform_dir}/src/*.cpp)

add_library(uxs SHARED .clang-format ${uxs_headers} ${uxs_sources}
                       ${uxs_platform_sources})

if(ZLIB_DEP OR LIBZIP_DEP)
  add_dependencies(uxs ${ZLIB_DEP} ${LIBZIP_DEP})
endif()

set(UXS_CUSTOM_CONFIG "\n")

if(USE_ZLIB)
  if(ZLIB_DEP)
    add_dependencies(uxs ${ZLIB_DEP})
  endif()
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_ZLIB 1\n")
  target_include_directories(uxs PRIVATE ${ZLIB_INCLUDE_DIR})
endif()

if(USE_LIBZIP)
  if(LIBZIP_DEP)
    add_dependencies(uxs ${LIBZIP_DEP})
  endif()
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_LIBZIP 1\n")
  target_include_directories(uxs PRIVATE ${LIBZIP_INCLUDE_DIR})
  target_link_libraries(uxs PRIVATE ${LIBZIP_LIBRARY})
endif()

if(USE_ZLIB OR USE_LIBZIP)
  target_link_libraries(uxs PRIVATE ${ZLIB_LIBRARY})
endif()

set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#ifndef NDEBUG\n")
set(UXS_CUSTOM_CONFIG
    "${UXS_CUSTOM_CONFIG}#  define UXS_DEBUG_REDUCED_BUFFERS 1\n")
if(NOT MSVC)
  set(UXS_CUSTOM_CONFIG
      "${UXS_CUSTOM_CONFIG}#  define UXS_ITERATOR_DEBUG_LEVEL 2\n")
endif()
set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#endif\n")

generate_export_header(
  uxs
  EXPORT_FILE_NAME
  ${PROJECT_BINARY_DIR}/generated/uxs/config.h
  INCLUDE_GUARD_NAME
  UXS_CONFIG_H
  CUSTOM_CONTENT_FROM_VARIABLE
  UXS_CUSTOM_CONFIG)

target_include_directories(uxs PRIVATE include ${PROJECT_BINARY_DIR}/generated)

# ##############################################################################
# Setup `uxs` installation

install(TARGETS uxs LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
                            COMPONENT library)

if(WIN32)
  install(TARGETS uxs RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                              COMPONENT binary)
endif()

install(
  DIRECTORY include
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  COMPONENT headers)

install(
  DIRECTORY cmake/uxs
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/cmake
  COMPONENT headers)

install(
  FILES ${PROJECT_BINARY_DIR}/generated/uxs/config.h
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/uxs
  COMPONENT share)

# ##############################################################################
# Auxiliary

include("cmake/uxs/macros.cmake")
add_compile_defs_and_include_dirs_targets(uxs ${CMAKE_CURRENT_SOURCE_DIR})
