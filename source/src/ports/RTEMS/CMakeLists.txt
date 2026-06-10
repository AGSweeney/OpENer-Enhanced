opener_common_includes()
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
  message(STATUS "RTEMS platform configured as library-only (linked by LeapOS-Gateway).")
endif()
