#[[*****************************************************************************
* Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
******************************************************************************]]

cmake_minimum_required(VERSION 3.9)

project("MaximInterface" VERSION 2.0)
set(CMAKE_UNITY_BUILD NO)
option(BUILD_SHARED_LIBS "Build ${PROJECT_NAME} as a shared library." ON)
if(pigpio_FOUND)
	set(MaximInterface_Platforms "pigpio" CACHE STRING "List of platforms to build. Available platforms are xx and pigpio.")
elseif(i2cxx_FOUND)
	set(MaximInterface_Platforms "xx" CACHE STRING "List of platforms to build. Available platforms are xx and pigpio.")
else()
	message(SEND_ERROR "Unknown platform for MAxim Interface")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Search in parent directory for includes.
get_filename_component(baseIncludeDir ${CMAKE_SOURCE_DIR} DIRECTORY)
include_directories(${baseIncludeDir})

# Prepend a path to all items in a list.
macro(prependPath var path)
	unset(${var})
	foreach(arg ${ARGN})
		list(APPEND ${var} "${path}/${arg}")
	endforeach()
endmacro(prependPath)

# Add base components.
set(resourceDirs "MaximInterfaceDevices" "MaximInterfaceCore")
prependPath(DevicesSources "MaximInterfaceDevices"
	"DS18B20.cpp"
	"DS28C36_DS2476.cpp"
	"DS28C39.cpp"
	"DS28C40.cpp"
	"DS28E15_22_25.cpp"
	"DS28E16.cpp"
	"DS28E17.cpp"
	"DS28E38.cpp"
	"DS28E39.cpp"
	"DS28E83_DS28E84.cpp"
	"DS1920.cpp"
	"DS2413.cpp"
	"DS2431.cpp"
	"DS2465.cpp"
	"DS2480B.cpp"
	"DS2482_DS2484.cpp"
	"DS9400.cpp"
	"DS9481P_300.cpp")

prependPath(UtilitiesSources "MaximInterfaceCore"
	"Crc.cpp"
	"Ecc256.cpp"
	"Error.cpp"
	"HexString.cpp"
	"RunCommand.cpp"
	"system_error.cpp"
	"I2CMaster.cpp"
	"I2CMasterDecorator.cpp"
	"LoggingI2CMaster.cpp"
	"LoggingOneWireMaster.cpp"
	"LoggingSleep.cpp"
	"OneWireMaster.cpp"
	"OneWireMasterDecorator.cpp"
	"RomCommands.cpp"
	"SelectRom.cpp"
	"SleepDecorator.cpp"
	"Uart.cpp")

add_library(${PROJECT_NAME} STATIC ${DevicesSources} ${UtilitiesSources})
#cotire(${PROJECT_NAME}) #speed up if possible using cotire
# Add selected platforms.
foreach(platform ${MaximInterface_Platforms})
	if(platform STREQUAL "pigpio")
		list(APPEND resourceDirs "Platforms/pigpio")
		prependPath(pigpioSources "Platforms/pigpio"
			"I2CMaster.cpp"
			"../Sleep.cpp")
		target_sources(${PROJECT_NAME} PRIVATE ${pigpioSources})
	elseif(platform STREQUAL "xx")
		list(APPEND resourceDirs "Platforms/i2cxx")
		prependPath(xxSources "Platforms/i2cxx"
			"I2CMaster.cpp"
			"../Sleep.cpp")
		target_sources(${PROJECT_NAME} PRIVATE ${xxSources})
	else()
		message(SEND_ERROR "\"${platform}\" is not a supported platform.")
	endif()
endforeach()

# Set up build and install exports.
target_include_directories(${PROJECT_NAME} INTERFACE
	$<BUILD_INTERFACE:${baseIncludeDir}>
	$<INSTALL_INTERFACE:.>)
#foreach(resourceDir ${resourceDirs})
#	get_filename_component(resourceInstallDir "${PROJECT_NAME}/${resourceDir}" DIRECTORY)
#	install(DIRECTORY ${resourceDir} DESTINATION ${resourceInstallDir}
#		FILES_MATCHING PATTERN "*.h*" PATTERN "*.dll")
#endforeach()
#install(TARGETS ${PROJECT_NAME} EXPORT "${PROJECT_NAME}Config"
#	ARCHIVE DESTINATION $<CONFIG>
#	LIBRARY DESTINATION $<CONFIG>
#	RUNTIME DESTINATION $<CONFIG>)
# install(EXPORT "${PROJECT_NAME}Config" DESTINATION "cmake")
export(TARGETS ${PROJECT_NAME} FILE "${PROJECT_NAME}Config.cmake")

# Set up source groups.
get_property(${PROJECT_NAME}Sources TARGET ${PROJECT_NAME} PROPERTY SOURCES)
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${${PROJECT_NAME}Sources})

#cotire(${PROJECT_NAME})
