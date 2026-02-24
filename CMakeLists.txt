cmake_minimum_required(VERSION 3.21)

project(
  uxs
  DESCRIPTION "Utilities and eXtensionS (UXS) for Standard C++ Library"
  LANGUAGES CXX)

include(ExternalProject)
include(GenerateExportHeader)
include(GNUInstallDirs)
include("cmake/macros.cmake")

option(UXS_USE_ZLIB "Use ZLib" ON)
option(UXS_USE_LIBZIP "Use LibZip" ON)
option(UXS_DO_INSTALL "Install library" ON)
option(UXS_DO_INSTALL_SDK "Install library SDK" ON)

if(MSVC)
  set(UXS_DEFAULT_ITERATOR_DEBUG_LEVEL _ITERATOR_DEBUG_LEVEL)
else()
  set(UXS_DEFAULT_ITERATOR_DEBUG_LEVEL 2)
endif()

set(UXS_ITERATOR_DEBUG_LEVEL
    ${UXS_DEFAULT_ITERATOR_DEBUG_LEVEL}
    CACHE STRING "Iterator debug level for Debug")
set(UXS_DEBUG_REDUCED_BUFFERS
    0
    CACHE STRING "Use reduced buffers for Debug")

set(INSTALL_WIN32_SYMBOLS_DIR
    ${CMAKE_INSTALL_BINDIR}
    CACHE STRING "Default directory for installation of symbol files")

set(CMAKE_CXX_STANDARD
    20
    CACHE STRING "Default C++ standard")
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

if((UXS_USE_ZLIB OR UXS_USE_LIBZIP) AND NOT ZLIB_LIBRARY)
  if(NOT ZLIB_SOURCE_DIR)
    set(ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib.git")
    set(ZLIB_GIT_TAG da607da739fa6047df13e66a2af6b8bec7c2a498)
  endif()
  set(ZLIB_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib/${CONFIG_SUBDIR})
  set(ZLIB_INCLUDE_DIR ${ZLIB_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})
  set(ZLIB_LIBRARY_NAME
      $<IF:$<BOOL:${WIN32}>,zs$<$<CONFIG:Debug>:d>.lib,libz.a>)
  set(ZLIB_LIBRARY
      ${ZLIB_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/${ZLIB_LIBRARY_NAME})
  set(UXS_ZLIB_DEP libz)
  ExternalProject_Add(
    libz
    GIT_REPOSITORY ${ZLIB_GIT_REPOSITORY}
    GIT_TAG ${ZLIB_GIT_TAG}
    SOURCE_DIR ${ZLIB_SOURCE_DIR}
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
    BUILD_BYPRODUCTS ${ZLIB_LIBRARY}
    CMAKE_CACHE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${ZLIB_INSTALL_DIR}
    CMAKE_ARGS -DZLIB_BUILD_TESTING=Off #
               -DZLIB_BUILD_SHARED=Off #
               -DCMAKE_POSITION_INDEPENDENT_CODE=On #
               ${EXT_PROJECT_CMAKE_ARGS})
endif()

# ##############################################################################
# Prepare `libzip` library

if(UXS_USE_LIBZIP AND NOT LIBZIP_LIBRARY)
  if(NOT LIBZIP_SOURCE_DIR)
    set(LIBZIP_GIT_REPOSITORY "https://github.com/nih-at/libzip.git")
    set(LIBZIP_GIT_TAG 8694564b8e636f9a42b7ca8bd3647aa0c1d0ca0b)
  endif()
  set(LIBZIP_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libzip/${CONFIG_SUBDIR})
  set(LIBZIP_INCLUDE_DIR ${LIBZIP_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})
  set(LIBZIP_LIBRARY_NAME $<IF:$<BOOL:${WIN32}>,zip.lib,libzip.a>)
  set(LIBZIP_LIBRARY
      ${LIBZIP_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/${LIBZIP_LIBRARY_NAME})
  set(UXS_LIBZIP_DEP libzip)
  ExternalProject_Add(
    libzip
    DEPENDS ${UXS_ZLIB_DEP}
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
               ${EXT_PROJECT_CMAKE_ARGS})
endif()

# ##############################################################################
# Add `uxs` build target

if(WIN32)
  set(UXS_PLATFORM_DIR platform/win)
elseif(UNIX)
  set(UXS_PLATFORM_DIR platform/posix)
endif()
file(GLOB_RECURSE UXS_HEADERS include/*.h)
file(GLOB_RECURSE UXS_SOURCES src/*.h;src/*.cpp)
file(GLOB_RECURSE UXS_PLATFORM_SOURCES
     ${UXS_PLATFORM_DIR}/src/*.h;${UXS_PLATFORM_DIR}/src/*.cpp)

add_library(uxs SHARED ${UXS_HEADERS} ${UXS_SOURCES} ${UXS_PLATFORM_SOURCES})

if(UXS_ZLIB_DEP)
  add_dependencies(uxs ${UXS_ZLIB_DEP})
endif()

if(UXS_LIBZIP_DEP)
  add_dependencies(uxs ${UXS_LIBZIP_DEP})
endif()

set(UXS_CUSTOM_CONFIG "\n")

if(UXS_USE_ZLIB)
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_ZLIB 1\n")
  target_include_directories(uxs PRIVATE ${ZLIB_INCLUDE_DIR})
endif()

if(UXS_USE_LIBZIP)
  set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#define UXS_USE_LIBZIP 1\n")
  target_include_directories(uxs PRIVATE ${LIBZIP_INCLUDE_DIR})
  target_link_libraries(uxs PRIVATE ${LIBZIP_LIBRARY})
endif()

if(UXS_USE_ZLIB OR UXS_USE_LIBZIP)
  target_link_libraries(uxs PRIVATE ${ZLIB_LIBRARY})
endif()

set(UXS_CUSTOM_CONFIG "${UXS_CUSTOM_CONFIG}#ifndef NDEBUG\n")
set(UXS_CUSTOM_CONFIG
    "${UXS_CUSTOM_CONFIG}#  define UXS_ITERATOR_DEBUG_LEVEL ${UXS_ITERATOR_DEBUG_LEVEL}\n"
)
set(UXS_CUSTOM_CONFIG
    "${UXS_CUSTOM_CONFIG}#  define UXS_DEBUG_REDUCED_BUFFERS ${UXS_DEBUG_REDUCED_BUFFERS}\n"
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

target_compile_options(uxs PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus
                                  /utf-8>)

target_include_directories(uxs PUBLIC include ${PROJECT_BINARY_DIR}/generated)

add_library(UXS::UXS ALIAS uxs)

# ##############################################################################
# Setup `uxs` installation

if(UXS_DO_INSTALL)
  if(NOT UXS_DO_INSTALL_SDK)
    install(
      TARGETS uxs
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
  else()
    install(
      TARGETS uxs
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries-dev)

    install(
      DIRECTORY include/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      COMPONENT headers)

    install(
      FILES ${PROJECT_BINARY_DIR}/generated/uxs/config.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uxs
      COMPONENT headers)
  endif()
  install_win32_pdb(uxs)
endif()

# ##############################################################################
# Auxiliary

add_compile_defs_and_include_dirs_targets(uxs ${CMAKE_CURRENT_SOURCE_DIR})
