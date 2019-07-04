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

include_guard()

macro(initializeClr)
	if(NOT MSVC)
		message(SEND_ERROR "MSVC must be used for CLR compilation.")
	endif()
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa /DNOMINMAX")
endmacro(initializeClr)

function(enableClr target dotnetFrameworkVersion)
	set(dotnetFrameworkDir
		"C:/Program Files (x86)/Reference Assemblies/Microsoft/Framework/.NETFramework/${dotnetFrameworkVersion}")
	set(ARGN "mscorlib.dll" ${ARGN})
	unset(dotnetReferences)
	foreach(reference ${ARGN})
		if(NOT IS_ABSOLUTE ${reference})
			set(reference "${dotnetFrameworkDir}/${reference}")
		endif()
		list(APPEND dotnetReferences ${reference})
	endforeach()

	if(CMAKE_GENERATOR MATCHES "Visual Studio")
		if(CMAKE_GENERATOR MATCHES "2005" OR CMAKE_GENERATOR MATCHES "2008")
			message(SEND_ERROR "Visual Studio 2010 or later is required to build CLR target \"${target}\".")
		endif()
		
		set_property(TARGET ${target} PROPERTY VS_GLOBAL_CLRSupport "true")
		set_property(TARGET ${target} PROPERTY
			VS_DOTNET_TARGET_FRAMEWORK_VERSION "${dotnetFrameworkVersion}")
		set_property(TARGET ${target} PROPERTY VS_DOTNET_REFERENCES ${dotnetReferences})
	else()
		unset(dotnetReferenceArgs)
		foreach(reference ${dotnetReferences})
			list(APPEND dotnetReferenceArgs "/FU${reference}")
		endforeach()
		set_property(TARGET ${target} APPEND PROPERTY COMPILE_OPTIONS
			"/AI${dotnetFrameworkDir}"
			"/clr"
			${dotnetReferenceArgs}
			"/clr:nostdlib")
	endif()
endfunction(enableClr)