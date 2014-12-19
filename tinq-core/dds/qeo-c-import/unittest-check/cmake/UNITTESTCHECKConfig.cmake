# Package dependencies
find_package (CHECK REQUIRED)
find_package (DL REQUIRED)

# Try to resolve the include dir and the library
find_path (UNITTESTCHECK_INCLUDE_DIR unittest/unittest.h)
find_program (UNITTESTCHECK_BINARY unittest)

# Verify if we have found unittestcheck and its include dir
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (UNITTESTCHECK DEFAULT_MSG
                                   UNITTESTCHECK_BINARY UNITTESTCHECK_INCLUDE_DIR)

# Set some more project specific variables
if (UNITTESTCHECK_FOUND)
    set (UNITTESTCHECK_INCLUDE_DIRS ${UNITTESTCHECK_INCLUDE_DIR} ${CHECK_INCLUDE_DIRS})
    set (UNITTESTCHECK_BINARIES ${UNITTESTCHECK_BINARY})
    set (UNITTESTCHECK_LIBRARIES ${CHECK_LIBRARIES})
else (UNITTESTCHECK_FOUND)
    set (UNITTESTCHECK_INCLUDE_DIRS)
    set (UNITTESTCHECK_BINARIES)
    set (UNITTESTCHECK_LIBRARIES)
endif (UNITTESTCHECK_FOUND)

mark_as_advanced (UNITTESTCHECK_BINARY 
                  UNITTESTCHECK_BINARIES
                  UNITTESTCHECK_LIBRARIES
                  UNITTESTCHECK_INCLUDE_DIR
                  UNITTESTCHECK_INCLUDE_DIRS)