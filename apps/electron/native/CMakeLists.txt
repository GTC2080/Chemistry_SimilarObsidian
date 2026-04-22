cmake_minimum_required(VERSION 3.21)
project(kernel_host_binding LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KERNEL_BUILD_DIR "" CACHE PATH "Path to the sealed kernel build tree")
set(NODE_HEADERS_DIR "" CACHE PATH "Path to the Node headers cache root")
set(NODE_ADDON_API_DIR "" CACHE PATH "Path to node-addon-api headers")
set(ELECTRON_NODE_LIB "" CACHE FILEPATH "Path to Electron's Windows node.lib")

if(NOT KERNEL_BUILD_DIR)
  message(FATAL_ERROR "KERNEL_BUILD_DIR must be provided.")
endif()

if(NOT NODE_HEADERS_DIR)
  message(FATAL_ERROR "NODE_HEADERS_DIR must be provided.")
endif()

if(NOT NODE_ADDON_API_DIR)
  message(FATAL_ERROR "NODE_ADDON_API_DIR must be provided.")
endif()

if(NOT ELECTRON_NODE_LIB)
  message(FATAL_ERROR "ELECTRON_NODE_LIB must be provided.")
endif()

add_library(
  kernel_host_binding
  MODULE
    src/kernel_host_binding.cc
    src/win_delay_load_hook.cc
)

target_include_directories(
  kernel_host_binding
  PRIVATE
    ${NODE_HEADERS_DIR}/include/node
    ${NODE_ADDON_API_DIR}
    ../../../kernel/include
)

target_compile_definitions(kernel_host_binding PRIVATE NAPI_CPP_EXCEPTIONS)

if(MSVC)
  target_compile_options(kernel_host_binding PRIVATE /EHsc)
  target_link_options(kernel_host_binding PRIVATE /DELAYLOAD:node.exe)
endif()

target_link_libraries(
  kernel_host_binding
  PRIVATE
    ${ELECTRON_NODE_LIB}
    $<$<CONFIG:Debug>:${KERNEL_BUILD_DIR}/src/Debug/chem_kernel.lib>
    $<$<CONFIG:Release>:${KERNEL_BUILD_DIR}/src/Release/chem_kernel.lib>
    $<$<CONFIG:RelWithDebInfo>:${KERNEL_BUILD_DIR}/src/RelWithDebInfo/chem_kernel.lib>
    $<$<CONFIG:MinSizeRel>:${KERNEL_BUILD_DIR}/src/MinSizeRel/chem_kernel.lib>
    kernel32.lib
    user32.lib
    gdi32.lib
    winspool.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    comdlg32.lib
    advapi32.lib
    delayimp.lib
)

set_target_properties(
  kernel_host_binding
  PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR}/../build/Debug
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR}/../build/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/../build/Release
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/../build/Release
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_LIST_DIR}/../build/RelWithDebInfo
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_LIST_DIR}/../build/RelWithDebInfo
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_LIST_DIR}/../build/MinSizeRel
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_LIST_DIR}/../build/MinSizeRel
  )
