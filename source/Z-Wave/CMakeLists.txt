message(STATUS ${OpenZWave})  
include_directories(${CMAKE_SOURCE_DIR}/open-zwave/cpp/src/)

add_subdirectory(zwave_service)
add_subdirectory(zwave_tool)