cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vivid)
include("${VITASDK}/share/vita.cmake" REQUIRED)

find_package(SDL2 CONFIG REQUIRED)

set(VITA_APP_NAME "ViViD")
set(VITA_TITLEID  "VVID00001")
set(VITA_VERSION  "01.00")

add_subdirectory(module)

include_directories(
  module/include
)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/module)

add_executable(${PROJECT_NAME}
  src/main.c
)

#add_dependencies(${PROJECT_NAME} "vividk.skprx-self")
add_dependencies(${PROJECT_NAME} "vividk_all")

target_link_libraries(${PROJECT_NAME}
  SDL2::SDL2-static
  vividk_stub_weak
  -Wl,--whole-archive pthread -Wl,--no-whole-archive
  taihen_stub
  SceVshBridge_stub
  SceAppMgr_stub
  SceShellSvc_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys sce_sys
  FILE data data
  FILE ${CMAKE_CURRENT_BINARY_DIR}/module/vividk.skprx module/vividk.skprx
)
