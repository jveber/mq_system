include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(target mq_lib)

set(sources
    mq_lib.cpp
)

if (NOT SQLITE3_FOUND)
	message(FATAL_ERROR "sqlite3 library not found but it's now the only one supported by engine for logging")
endif (NOT SQLITE3_FOUND)

# Build library
add_library(${target} OBJECT ${sources})
set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)
if (${IPO_SUPPORTED})
	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()
#cotire(${target})
