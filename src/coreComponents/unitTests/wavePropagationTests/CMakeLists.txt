#
# Specify list of tests
#

set( gtest_geosx_tests
	testWavePropagation.cpp
   )

set( dependencyList gtest )

if ( GEOSX_BUILD_SHARED_LIBS )
  set (dependencyList ${dependencyList} geosx_core )
else()
  set (dependencyList ${dependencyList} ${geosx_core_libs} )
endif()

if ( ENABLE_CUDA )
  set( dependencyList ${dependencyList} cuda )
endif()

#
# Add gtest C++ based tests
#
foreach(test ${gtest_geosx_tests})
    get_filename_component( test_name ${test} NAME_WE )
    blt_add_executable( NAME ${test_name}
            SOURCES ${test}
            OUTPUT_DIR ${TEST_OUTPUT_DIRECTORY}
            DEPENDS_ON ${dependencyList}
            )

    # Guard to prevent GCC (version 8) from giving warnings due
    # to some sort of possible conversion from int to long unsigned.
    # See also discussion here: https://github.com/GEOSX/LvArray/pull/250
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.3.1)
        target_compile_options(${test_name} PRIVATE "-Wno-stringop-overflow")
      endif()
    endif()

    blt_add_test( NAME ${test_name}
            COMMAND ${test_name}
            )

endforeach()

# For some reason, BLT is not setting CUDA language for these source files
if ( ENABLE_CUDA )
  set_source_files_properties( ${gtest_geosx_tests} PROPERTIES LANGUAGE CUDA )
endif()
