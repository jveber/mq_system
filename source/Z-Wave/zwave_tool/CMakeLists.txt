# 
# External dependencies
# 

# find_package(THIRDPARTY REQUIRED)


# 
# Library name and options
# 

# Target name
set(target mq_zwave_tool)

# Exit here if required dependencies are not met
#message(STATUS "Lib ${target}")

# Set API export file and macro

set(sources
    ${target}.cpp
)

add_executable(${target} ${sources})
target_link_libraries(${target} ${OpenZWave} ${UDEV_LIBRARIES} ${CONFIG++_LIBRARY} pthread resolv)
set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)
if (${IPO_SUPPORTED})
	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()
#cotire(${target})
