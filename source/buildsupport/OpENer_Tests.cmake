#######################################
# Adds test includes                  #
#######################################
macro( add_test_includes )
  if(MSVC)
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FI\"${CPPUTEST_HOME}/include/CppUTest/MemoryLeakDetectorNewMacros.h\"" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FI\"${CPPUTEST_HOME}/include/CppUTest/MemoryLeakDetectorMallocMacros.h\"" )
  else()
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage -include ${CPPUTEST_HOME}/include/CppUTest/MemoryLeakDetectorNewMacros.h" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage -include ${CPPUTEST_HOME}/include/CppUTest/MemoryLeakDetectorMallocMacros.h" )
  endif()
  include_directories( ${CPPUTEST_HOME}/include ${OpENer_SOURCE_DIR}/tests)
endmacro( add_test_includes )
