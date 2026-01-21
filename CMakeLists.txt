cmake_minimum_required(VERSION 3.21)

project(
  uxs
  DESCRIPTION "Utilities and eXtensionS (UXS) for Standard C++ Library"
  LANGUAGES CXX)

include(ExternalProject)
include(GenerateExportHeader)
include("cmake/macros.cmake")

option(USE_ZLIB "Use ZLib" ON)
option(USE_LIBZIP "Use LibZip" ON)
option(USE_LIBCPP "Use LibC++" OFF)
option(USE_SANITIZERS_FOR_DEBUG "Use Sanitizers for Debug build" ON)
option(OPTION_TREAT_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(OPTION_EXPORT_COMPILE_DEFS_AND_INCLUDE_DIRS
       "Export compile definitions and include directories" OFF)
option(OPTION_DO_INSTALL "Install all library stuff" ON)

if(MSVC)
  set(DEFAULT_ITERATOR_DEBUG_LEVEL _ITERATOR_DEBUG_LEVEL)
else()
  set(DEFAULT_ITERATOR_DEBUG_LEVEL 2)
endif()

set(ITERATOR_DEBUG_LEVEL
    ${DEFAULT_ITERATOR_DEBUG_LEVEL}
    CACHE STRING "Iterator debug level for Debug build")
set(DEBUG_REDUCED_BUFFERS
    1
    CACHE STRING "Use reduced buffers for Debug build")

if(NOT SYMBOLS_DIR)
  set(SYMBOLS_DIR bin)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Using C++${CMAKE_CXX_STANDARD} standard")

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

foreach(var_name CMAKE_C_COMPILER CMAKE_C_FLAGS CMAKE_BUILD_TYPE)
  if(DEFINED ${var_name})
    list(APPEND EXT_PROJECT_CMAKE_ARGS "-D${var_name}=${${var_name}}")
  endif()
endforeach()

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(CONFIG_SUBDIR $<$<BOOL:${IS_MULTI_CONFIG}>:$<CONFIG>>)

# ##############################################################################
# Prepare `zlib` library

if((USE_ZLIB OR USE_LIBZIP) AND NOT ZLIB_LIBRARY)
  if(NOT ZLIB_SOURCE_DIR)
    set(ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib.git")
    set(ZLIB_GIT_TAG 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf)
  endif()
  set(ZLIB_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib/${CONFIG_SUBDIR})
  set(ZLIB_INCLUDE_DIR ${ZLIB_INSTALL_DIR}/include)
  set(ZLIB_LIBRARY_NAME
      $<IF:$<BOOL:${WIN32}>,zlibstatic$<$<CONFIG:Debug>:d>.lib,libz.a>)
  set(ZLIB_LIBRARY ${ZLIB_INSTALL_DIR}/lib/${ZLIB_LIBRARY_NAME})
  set(ZLIB_DEP libz)
  ExternalProject_Add(
    libz
    GIT_REPOSITORY ${ZLIB_GIT_REPOSITORY}
    GIT_TAG ${ZLIB_GIT_TAG}
    SOURCE_DIR ${ZLIB_SOURCE_DIR}
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
    BUILD_BYPRODUCTS ${ZLIB_LIBRARY}
    CMAKE_CACHE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${LIBZIP_INSTALL_DIR} #
      -DINSTALL_LIB_DIR:PATH=${ZLIB_INSTALL_DIR}/lib #
      -DINSTALL_BIN_DIR:PATH=${ZLIB_INSTALL_DIR}/bin #
      -DINSTALL_INC_DIR:PATH=${ZLIB_INSTALL_DIR}/include
    CMAKE_ARGS -DZLIB_BUILD_EXAMPLES=Off #
               -DSKIP_INSTALL_FILES=On #
               -DCMAKE_POSITION_INDEPENDENT_CODE=On #
               ${EXT_PROJECT_CMAKE_ARGS})
endif()

# ##############################################################################
# Prepare `libzip` library

if(USE_LIBZIP AND NOT LIBZIP_LIBRARY)
  if(NOT LIBZIP_SOURCE_DIR)
    set(LIBZIP_GIT_REPOSITORY "https://github.com/nih-at/libzip.git")
    set(LIBZIP_GIT_TAG a1364389320754b437234c30989e37e1881b4fef)
  endif()
  set(LIBZIP_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libzip/${CONFIG_SUBDIR})
  set(LIBZIP_INCLUDE_DIR ${LIBZIP_INSTALL_DIR}/include)
  set(LIBZIP_LIBRARY_NAME $<IF:$<BOOL:${WIN32}>,zip.lib,libzip.a>)
  set(LIBZIP_LIBRARY ${LIBZIP_INSTALL_DIR}/lib/${LIBZIP_LIBRARY_NAME})
  set(LIBZIP_DEP libzip)
  ExternalProject_Add(
    libzip
    DEPENDS ${ZLIB_DEP}
    GIT_REPOSITORY ${LIBZIP_GIT_REPOSITORY}
    GIT_TAG ${LIBZIP_GIT_TAG}
    SOURCE_DIR ${LIBZIP_SOURCE_DIR}
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libzip
    BUILD_BYPRODUCTS ${LIBZIP_LIBRARY}
    CMAKE_CACHE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${LIBZIP_INSTALL_DIR}
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
               -DCMAKE_INSTALL_LIBDIR=lib #
               ${EXT_PROJECT_CMAKE_ARGS})
endif()

# ##############################################################################
# Compilation flags

if(MSVC)
  add_compile_options(/Zc:__cplusplus /utf-8)
  if(OPTION_TREAT_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -pedantic)
  if(OPTION_TREAT_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
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

add_library(uxs SHARED ${uxs_headers} ${uxs_sources} ${uxs_platform_sources})

if(ZLIB_DEP)
  add_dependencies(uxs ${ZLIB_DEP})
endif()

if(LIBZIP_DEP)
  add_dependencies(uxs ${LIBZIP_DEP})
endif()

set(UXS_CUSTOM_CONFIG "\n")

if(USE_ZLIB)
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_ZLIB 1\n")
  target_include_directories(uxs PRIVATE ${ZLIB_INCLUDE_DIR})
endif()

if(USE_LIBZIP)
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_LIBZIP 1\n")
  target_include_directories(uxs PRIVATE ${LIBZIP_INCLUDE_DIR})
  target_link_libraries(uxs PRIVATE ${LIBZIP_LIBRARY})
endif()

if(USE_ZLIB OR USE_LIBZIP)
  target_link_libraries(uxs PRIVATE ${ZLIB_LIBRARY})
endif()

set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#ifndef NDEBUG\n")
set(UXS_CUSTOM_CONFIG
    "${UXS_CUSTOM_CONFIG}#  define UXS_ITERATOR_DEBUG_LEVEL ${ITERATOR_DEBUG_LEVEL}\n"
)
set(UXS_CUSTOM_CONFIG
    "${UXS_CUSTOM_CONFIG}#  define UXS_DEBUG_REDUCED_BUFFERS ${DEBUG_REDUCED_BUFFERS}\n"
)
set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#endif\n")

generate_export_header(
  uxs
  EXPORT_FILE_NAME
  ${PROJECT_BINARY_DIR}/generated/uxs/config.h
  INCLUDE_GUARD_NAME
  UXS_CONFIG_H
  CUSTOM_CONTENT_FROM_VARIABLE
  UXS_CUSTOM_CONFIG)

target_include_directories(uxs PUBLIC include ${PROJECT_BINARY_DIR}/generated)

# ##############################################################################
# Setup `uxs` installation

if(OPTION_DO_INSTALL)
  install(
    TARGETS uxs
    RUNTIME DESTINATION bin COMPONENT binaries
    LIBRARY DESTINATION lib COMPONENT libraries
    ARCHIVE DESTINATION lib COMPONENT libraries)
  install_win32_pdb(uxs)

  install(
    DIRECTORY include
    DESTINATION .
    COMPONENT headers)

  install(
    FILES ${PROJECT_BINARY_DIR}/generated/uxs/config.h
    DESTINATION include/uxs
    COMPONENT headers)
endif()

# ##############################################################################
# Auxiliary

add_compile_defs_and_include_dirs_targets(uxs ${CMAKE_CURRENT_SOURCE_DIR})
