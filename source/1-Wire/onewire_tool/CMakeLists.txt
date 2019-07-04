
# 
# External dependencies
# 

# find_package(THIRDPARTY REQUIRED)


# 
# Library name and options
# 

# Target name
set(target mq_onewire_tool)

# Exit here if required dependencies are not met
#message(STATUS "Lib ${target}")

# Set API export file and macro

set(sources ${target}.cpp)

add_executable(${target} ${sources})
target_link_libraries(${target} ${CONFIG++_LIBRARY} MaximInterface pthread)
if (pigpio_FOUND)
    target_link_libraries(${target} ${pigpiod_if2_LIBRARY})
elseif(i2cxx_FOUND)
    target_link_libraries(${target} i2cxx)
 else ()
    message(SNED_ERROR "Neither pigpio nor i2cxx found ... dunno what to link to onewire daemon")  # this should never happen
endif()

set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)

if (${IPO_SUPPORTED})
	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()

cotire(${target})