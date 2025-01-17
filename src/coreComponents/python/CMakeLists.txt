set( ENV{LD_LIBRARY_PATH}, ENV{LD_LIBRARY_PATH}:${PYTHON_DIR}/lib/ )

set( PYTHON_INTERP_INCLUDE_DIRS
     ${PYTHON_DIR}/include/python2.7/
     ${PYTHON_DIR}/lib/python2.7/site-packages/numpy/core/include/
   )


# If an explicit python post-processing target is not set, then try
# using the python being used with cmake.  Note: if the user does not have
# write permissions, the virtualenv package is required to create a new
# virtual environment
if( NOT PYTHON_POST_EXECUTABLE )
     if( ENABLE_PYGEOSX )
          set( PYTHON_POST_EXECUTABLE ${CMAKE_BINARY_DIR}/lib/PYGEOSX/bin/python CACHE PATH "" FORCE )
     else()
          set( PYTHON_POST_EXECUTABLE ${PYTHON_EXECUTABLE} CACHE PATH "" FORCE )
     endif()
endif()


blt_import_library( NAME python_interp
                    INCLUDES    ${PYTHON_INTERP_INCLUDE_DIRS}
                    HEADERS     ${PYTHON_INTERP_INCLUDE_DIRS}
                    HEADERS_OUTPUT_SUBDIR python_interp
                    LIBRARIES python2.7 
                  )


set( GEOSX_XML_TOOLS_BINS
     "${CMAKE_BINARY_DIR}/bin/preprocess_xml"
     "${CMAKE_BINARY_DIR}/bin/format_xml"
   )


add_custom_command( OUTPUT ${GEOSX_XML_TOOLS_BINS}
                    COMMAND bash ${CMAKE_SOURCE_DIR}/../scripts/setupPythonEnvironment.bash -p ${PYTHON_POST_EXECUTABLE} -b ${CMAKE_BINARY_DIR}/bin
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  )

add_custom_target( geosx_xml_tools 
                   DEPENDS ${GEOSX_XML_TOOLS_BINS} )

add_custom_target( geosx_xml_tools_test
                   COMMAND ${CMAKE_BINARY_DIR}/python/geosx/bin/test_geosx_xml_tools
                   COMMAND rm -r ${CMAKE_BINARY_DIR}/python/geosx_xml_tools_tests*
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/python
                   DEPENDS geosx_xml_tools 
                 )

add_custom_target( geosx_format_all_xml_files
                   COMMAND bash ${CMAKE_SOURCE_DIR}/../scripts/formatXMLFiles.bash -g ${CMAKE_BINARY_DIR}/bin/format_xml ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/../examples
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                   DEPENDS geosx_xml_tools 
                 )

