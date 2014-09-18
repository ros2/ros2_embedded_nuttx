cmake_minimum_required (VERSION 2.6)

project (cmock C)

# Package dependencies
find_package (CHECK REQUIRED)

# Set the include path
set (HDRS api/headers/)
include_directories (${HDRS} ${CHECK_INCLUDE_DIRS})

# Special CFLAGS and stuff
add_definitions ()

# The sources to be compiled
set (SOURCES 
    src/src/cmock.c
)

# Library rules
add_library (cmock SHARED ${SOURCES})
target_link_libraries (cmock ${CHECK_LIBRARIES})

# Install rules
install (TARGETS cmock 
		 RUNTIME DESTINATION bin 
		 LIBRARY DESTINATION lib 
		 COMPONENT cmock)
install (FILES ${HDRS}/unity.h src/src/cmock.h DESTINATION include)
install (FILES cmake/CMOCKConfig.cmake DESTINATION cmake)
