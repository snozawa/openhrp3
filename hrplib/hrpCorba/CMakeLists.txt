
# @author Shin'ichiro Nakaoka

include(${OPENHRP_IDL_DIR}/OpenHRP/IDL_FILE_BASENAMES.cmake)

set(idl_flags ${VERBOSE_FLAG} -bcxx -Wbuse_quotes -Wbh=.hh -Wbs=Sk.cpp -I${OPENRTM_IDL_DIR})

if( NOT ADDITIONAL_SYMBOL STREQUAL "")
  set(idl_flags ${idl_flags} -D ${ADDITIONAL_SYMBOL})
endif()


foreach(name ${IDL_FILE_BASENAMES})
  set(idl_files ${idl_files} ${OPENHRP_IDL_DIR}/OpenHRP/${name}.idl)
  set(idl_cpp_files ${idl_cpp_files} ${CMAKE_CURRENT_BINARY_DIR}/${name}Sk.cpp)
  set(idl_h_files ${idl_h_files} ${CMAKE_CURRENT_BINARY_DIR}/${name}.hh)
endforeach()

foreach(idl_basename ${IDL_FILE_BASENAMES})
  set(idl_file ${OPENHRP_IDL_DIR}/OpenHRP/${idl_basename}.idl)
  if(UNIX)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}.hh ${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}Sk.cpp
      COMMAND omniidl ${idl_flags} ${idl_file}
      DEPENDS ${idl_files}
      )
  elseif(WIN32)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}.hh ${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}Sk.cpp
      COMMAND set PATH= "${OMNIORB_DIR}/bin/x86_win32"\;%PATH%
      COMMAND omniidl ${idl_flags} ${idl_file}
      DEPENDS ${idl_files}
      )
  endif()
endforeach()

set(target hrpCorbaStubSkel-${OPENHRP_LIBRARY_VERSION})

add_library(${target} STATIC ${idl_cpp_files})

if(UNIX)
  set_target_properties(${target} PROPERTIES COMPILE_FLAGS -fPIC)
endif()

if(WIN32)
  set_target_properties(${target} PROPERTIES DEBUG_POSTFIX d)
endif() 

if(WIN32)
install(TARGETS ${target}
	RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin CONFIGURATIONS Release Debug
	LIBRARY DESTINATION ${PROJECT_BINARY_DIR}/lib CONFIGURATIONS Release Debug
	ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/lib CONFIGURATIONS Release Debug
)
endif()

install(TARGETS ${target}
	RUNTIME DESTINATION bin CONFIGURATIONS Release Debug
	LIBRARY DESTINATION lib CONFIGURATIONS Release Debug
	ARCHIVE DESTINATION lib CONFIGURATIONS Release Debug
)

set(given_headers ORBwrap.h)

install(FILES ${given_headers} ${idl_h_files} DESTINATION ${RELATIVE_HEADERS_INSTALL_PATH}/hrpCorba)

