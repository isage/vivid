cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vividk)
include("${VITASDK}/share/vita.cmake" REQUIRED)

add_executable(${PROJECT_NAME}
  src/main.c
)

target_link_libraries(${PROJECT_NAME}
  SceDebugForDriver_stub
  SceCtrlForDriver_stub
  SceCpuForDriver_stub
  SceKernelSuspendForDriver_stub
  SceModulemgrForDriver_stub
  SceSysclibForDriver_stub
  SceSysmemForDriver_stub
  SceSysconForDriver_stub
  SceThreadmgrForDriver_stub
  SceUdcdForDriver_stub
  SceBtForDriver_stub
  SceOledForDriver_stub_weak
  SceLcdForDriver_stub_weak
  taihenForKernel_stub
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}
  CONFIG vividk.yml
  UNSAFE
)

vita_create_stubs(stubs vividk ${CMAKE_CURRENT_SOURCE_DIR}/vividk.yml KERNEL)

add_custom_target(vividk_all
  ALL
  DEPENDS vividk.skprx-self
  DEPENDS libvividk_stub.a
  DEPENDS libvividk_stub_weak.a
)
