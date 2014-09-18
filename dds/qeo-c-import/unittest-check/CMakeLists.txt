cmake_minimum_required (VERSION 2.6)

project (unittestcheck C)

# Package dependencies
find_package (CHECK REQUIRED)
find_package (DL REQUIRED)

# Set the include path
set (HDRS api/headers/)
include_directories (${HDRS} ${CHECK_INCLUDE_DIRS})

# Special CFLAGS and stuff
find_file (MCHECK_SUPPORT mcheck.h) 
if (MCHECK_SUPPORT)
    add_definitions (-DMCHECK_SUPPORT)
endif (MCHECK_SUPPORT)

# The sources to be compiled
set (SOURCES 
    src/unittest.c
)

# Library rules
add_executable (unittest ${SOURCES})
target_link_libraries (unittest ${CHECK_LIBRARIES} ${DL_LIBRARIES})

# Install rules
install (TARGETS unittest 
		 RUNTIME DESTINATION bin 
		 LIBRARY DESTINATION lib 
		 COMPONENT unittest)
install (DIRECTORY ${HDRS} DESTINATION include)
install (FILES cmake/UNITTESTCHECKConfig.cmake DESTINATION cmake)
