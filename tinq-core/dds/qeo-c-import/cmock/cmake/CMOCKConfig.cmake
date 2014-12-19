find_path (CMOCK_INCLUDE_DIR cmock.h)
find_library (CMOCK_LIBRARY cmock)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (CMOCK DEFAULT_MSG
                                   CMOCK_LIBRARY CMOCK_INCLUDE_DIR)

if (CMOCK_FOUND)
    set (CMOCK_INCLUDE_DIRS ${CMOCK_INCLUDE_DIR})
    set (CMOCK_LIBRARIES ${CMOCK_LIBRARY})
else (CMOCK_FOUND)
    set (CMOCK_INCLUDE_DIRS)
    set (CMOCK_LIBRARIES)
endif (CMOCK_FOUND)

mark_as_advanced (CMOCK_LIBRARY 
                  CMOCK_LIBRARIES
                  CMOCK_INCLUDE_DIR
                  CMOCK_INCLUDE_DIRS)