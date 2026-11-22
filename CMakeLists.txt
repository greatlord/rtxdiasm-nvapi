cmake_minimum_required(VERSION 3.21)
project(rtxdiasm_nvapi VERSION 0.1.0 LANGUAGES C)

if(NOT WIN32)
  message(FATAL_ERROR "rtxdiasm-nvapi is Windows-only (produces nvapi64.dll / nvapi.dll).")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ACUDA_NVAPI_ARCH x86)
  set(ACUDA_NVAPI_NAME nvapi)
else()
  set(ACUDA_NVAPI_ARCH x64)
  set(ACUDA_NVAPI_NAME nvapi64)
endif()
set(ACUDA_NVAPI_OUT "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ACUDA_NVAPI_ARCH}")

set(ACUDA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../acuda" CACHE PATH "Path to acuda source root")
set(ACUDA_INCLUDE_DIR "${ACUDA_ROOT}/include" CACHE PATH "Path to acuda public headers")
set(ACUDA_CUDA_DRIVER_LIBRARY "" CACHE FILEPATH "Path to acuda CUDA driver import/shared library (cuda/nvcuda) for standalone builds")

add_library(acuda_nvapi SHARED
  src/discovery.c
  src/nvapi.c
)
target_include_directories(acuda_nvapi PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external/nvapi
  ${ACUDA_INCLUDE_DIR}
)
if(TARGET acuda_driver)
  target_link_libraries(acuda_nvapi PRIVATE acuda_driver)
elseif(ACUDA_CUDA_DRIVER_LIBRARY)
  target_link_libraries(acuda_nvapi PRIVATE "${ACUDA_CUDA_DRIVER_LIBRARY}")
else()
  find_library(ACUDA_CUDA_DRIVER_FOUND
    NAMES cuda nvcuda libcuda
    HINTS
      "${ACUDA_ROOT}/bin/release/x64"
      "${ACUDA_ROOT}/bin/release"
      "${ACUDA_ROOT}/bin/debug/x64"
      "${ACUDA_ROOT}/bin/debug"
      "${ACUDA_ROOT}/build/release"
      "${ACUDA_ROOT}/build/debug"
      "${ACUDA_ROOT}/out/win-x64"
      "${ACUDA_ROOT}/../../out/win-x64"
    PATH_SUFFIXES lib bin
  )
  if(ACUDA_CUDA_DRIVER_FOUND)
    target_link_libraries(acuda_nvapi PRIVATE "${ACUDA_CUDA_DRIVER_FOUND}")
  else()
    message(WARNING "acuda_nvapi: acuda CUDA driver not found (no acuda_driver target and no cuda/nvcuda library). Set ACUDA_CUDA_DRIVER_LIBRARY for standalone builds.")
  endif()
endif()
target_compile_definitions(acuda_nvapi PRIVATE
  _CRT_SECURE_NO_WARNINGS
  WIN32_LEAN_AND_MEAN
)
target_compile_options(acuda_nvapi PRIVATE /W3)
set_target_properties(acuda_nvapi PROPERTIES
  OUTPUT_NAME ${ACUDA_NVAPI_NAME}
  RUNTIME_OUTPUT_DIRECTORY "${ACUDA_NVAPI_OUT}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ACUDA_NVAPI_OUT}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ACUDA_NVAPI_OUT}"
  ARCHIVE_OUTPUT_DIRECTORY "${ACUDA_NVAPI_OUT}"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${ACUDA_NVAPI_OUT}"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${ACUDA_NVAPI_OUT}"
)

option(ACUDA_NVAPI_BUILD_TESTS "Build acuda_nvapi test kit" ON)
if(ACUDA_NVAPI_BUILD_TESTS)
  add_executable(acuda_nvapi_test tests/test_nvapi.c)
  target_compile_definitions(acuda_nvapi_test PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_target_properties(acuda_nvapi_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ACUDA_NVAPI_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ACUDA_NVAPI_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ACUDA_NVAPI_OUT}"
  )
  add_dependencies(acuda_nvapi_test acuda_nvapi)
endif()