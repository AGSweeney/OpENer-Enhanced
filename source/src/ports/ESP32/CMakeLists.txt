#######################################
# Add common includes                 #
#######################################
opener_common_includes()

#######################################
# Add platform specific things        #
#######################################
opener_platform_support("INCLUDES")
add_subdirectory(sample_application)

set(PLATFORMLIBNAME ${OpENer_PLATFORM}PLATFORM)
set(PLATFORM_SPEC_SRC networkhandler.c opener_error.c networkconfig.c opener.c)

add_library(${PLATFORMLIBNAME} ${PLATFORM_SPEC_SRC})

target_link_libraries(${PLATFORMLIBNAME}
  PLATFORM_GENERIC
  CIP
  ENET_ENCAP
  Utils
  NVDATA
  SAMPLE_APP
  ${OpENer_CIP_OBJECTS}
)

if(NOT OpENer_TESTS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    add_executable(OpENer main.c)
    target_link_libraries(OpENer PLATFORM_GENERIC ${PLATFORMLIBNAME} CIP Utils SAMPLE_APP ENET_ENCAP NVDATA ${OpENer_CIP_OBJECTS})
  else()
    message(STATUS "ESP32 platform configured as library-only (no main.c in source/src/ports/ESP32).")
  endif()
endif()
