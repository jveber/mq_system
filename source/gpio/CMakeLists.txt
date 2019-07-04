include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(target gpioxx)

set(sources
    gpioxx.cpp
)

# Build library
add_library(${target} OBJECT ${sources})
set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)
#if (${IPO_SUPPORTED})
#	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
#endif()
#cotire(${target})
