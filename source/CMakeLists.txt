
add_custom_target(uninstall 
	COMMAND rm -f /etc/mq_system/system.conf
	COMMAND rm -rf /var/log/mq_system
	COMMAND rm -f /var/db/mq_system.db
	COMMAND rm -f /var/db/mq_exe_system.db
	COMMAND rm -f /var/db/mq_log.db
	COMMAND rm -rf /etc/mq_system
)
# 
# Configuration for all sub-projects
# 
configure_file(config.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mq_lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 
# Sub-projects
# 

# Libraries
set(IDE_FOLDER "")

add_subdirectory(mq_lib)

if (gpiocxx_FOUND AND NOT pigpio_FOUND)
	add_subdirectory(gpio)
endif()

if (i2cxx_FOUND AND NOT pigpio_FOUND)
	add_subdirectory(i2c)
endif()

option(MQ_DB_MODULE "Build any of available database modules" ON)

if (NOT SQLITE3_FOUND)
	if (MQ_DB_MODULE)
		message(STATUS "sqlite3 library not found can't build sqlite database module")
	endif()
else ()
	if (MQ_DB_MODULE)
		add_subdirectory(db_sqlite)
	endif ()
endif ()

option(MQ_DHT_MODULE "Build DHT module" ON)
option(MQ_UNIPI_MODULE "Build Unipi module" ON)
option(MQ_1_WIRE_MODULE "Build 1-Wire module" ON)

if (MQ_DHT_MODULE)
	if (pigpio_FOUND OR gpiocxx_FOUND)
		add_subdirectory(dht_service)
	else()
		message(STATUS "pigpio nor dev/gpio found - unable to build DHT module")
	endif()
endif()

if (MQ_1_WIRE_MODULE)
	if (pigpio_FOUND OR i2cxx_FOUND)
		add_subdirectory(1-Wire)
	else ()
		message(STATUS "pigpio nor dev/i2c found - unable to build 1-Wire module")
	endif()
endif()

if (MQ_UNIPI_MODULE)
	if (pigpio_FOUND OR (i2cxx_FOUND AND  gpiocxx_FOUND))
		add_subdirectory(unipi_service)
	else ()
		message(STATUS "pigpio nor dev/i2c and gpio found - unable to build UniPi module")
	endif()
endif()


find_library(OpenZWave NAMES libopenzwave.a HINTS ${CMAKE_SOURCE_DIR}/open-zwave)

CMAKE_DEPENDENT_OPTION(MQ_ZWAVE_MODULE "Build Z-Wave module" ON "UDEV_FOUND; OpenZWave" OFF)
if (MQ_ZWAVE_MODULE)
	if (NOT UDEV_FOUND) 
		message(STATUS "udev library not found cant build zwave module")
	else ()
		if (OpenZWave)
			add_subdirectory(Z-Wave)
		else()
			message(STATUS "OpenZWave library not found cant build zwave module")
		endif(OpenZWave)
	endif(NOT UDEV_FOUND)
endif ()

option (MQ_EXE_MODULE "Build Execution module" ON)

if (MQ_EXE_MODULE)
	include_directories(${LUA_INCLUDE_DIR})
	add_subdirectory(exe_service)
endif ()

install(DIRECTORY DESTINATION /var/log/mq_system)
install(DIRECTORY DESTINATION /var/db/)

if (DAEMON_MANAGER EQUAL 1)
	file(MAKE_DIRECTORY /var/run/)
endif()

install(FILES ${CMAKE_SOURCE_DIR}/data/system.conf DESTINATION /etc/mq_system/ ) # this one should also make directory