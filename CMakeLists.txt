# 
# CMake options
# 

# CMake version
cmake_minimum_required(VERSION 3.9.4 FATAL_ERROR)
set(CMAKE_VERBOSE_MAKEFILE ON)
#
# Configure CMake environment
#

# Set policies
cmake_policy(SET CMP0028 NEW) # ENABLE CMP0028: Double colon in target name means ALIAS or IMPORTED target.
cmake_policy(SET CMP0054 NEW) # ENABLE CMP0054: Only interpret if() arguments as variables or keywords when unquoted.
cmake_policy(SET CMP0042 NEW) # ENABLE CMP0042: MACOSX_RPATH is enabled by default.
cmake_policy(SET CMP0063 NEW) # ENABLE CMP0063: Honor visibility properties for all target types.
cmake_policy(SET CMP0069 NEW) # INTERPROCEDURAL_OPTIMIZATION is enforced when enabled.

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenerateExportHeader)
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
include(CMakeDependentOption)
message(STATUS "Using IPO ${IPO_SUPPORTED}")


set(WriterCompilerDetectionHeaderFound NOTFOUND)
include(WriteCompilerDetectionHeader OPTIONAL RESULT_VARIABLE WriterCompilerDetectionHeaderFound)

# Include custom cmake modules
include(cmake/GetGitRevisionDescription.cmake)
include(cmake/cotire.cmake)
include(CMakeDependentOption)

set (COTIRE_MINIMUM_NUMBER_OF_TARGET_SOURCES 2)
# 
# Project description and (meta) information
# 

# Get git revision
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
string(SUBSTRING "${GIT_SHA1}" 0 12 GIT_REV)
if(NOT GIT_SHA1)
    set(GIT_REV "0")
endif()

# Meta information about the project
set(META_PROJECT_NAME        "mq_system")
set(META_PROJECT_DESCRIPTION "CMake Project Template")
set(META_AUTHOR_ORGANIZATION "")
set(META_AUTHOR_DOMAIN       "")
set(META_AUTHOR_MAINTAINER   "")
set(META_VERSION_MAJOR       "0")
set(META_VERSION_MINOR       "1")
set(META_VERSION_PATCH       "0")
set(META_VERSION_REVISION    "${GIT_REV}")
set(META_VERSION             "${META_VERSION_MAJOR}.${META_VERSION_MINOR}.${META_VERSION_PATCH}")
set(META_NAME_VERSION        "${META_PROJECT_NAME} v${META_VERSION} (${META_VERSION_REVISION})")
set(META_CMAKE_INIT_SHA      "${GIT_SHA1}")

string(MAKE_C_IDENTIFIER ${META_PROJECT_NAME} META_PROJECT_ID)
string(TOUPPER ${META_PROJECT_ID} META_PROJECT_ID)

# Declare project
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(IDE_FOLDER "")

# Declare project
project(${META_PROJECT_NAME} C CXX)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Create version file
file(WRITE "${PROJECT_BINARY_DIR}/VERSION" "${META_NAME_VERSION}")

# Compiler settings and options 
include(cmake/CompileOptions.cmake)
set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-psabi -pipe")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-reorder -Wno-unused -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -ftree-vectorize") 
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2 -ftree-vectorize") 

# 
# Deployment/installation setup
# 

# Get project name
set(project ${META_PROJECT_NAME})

# Check for system dir install
set(SYSTEM_DIR_INSTALL FALSE)
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" OR "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
    set(SYSTEM_DIR_INSTALL TRUE)
endif()

# Installation paths
if(UNIX AND SYSTEM_DIR_INSTALL)
    # Install into the system (/usr/bin or /usr/local/bin)    
    set(INSTALL_DATA      "etc/${project}")       # /usr/[local]/share/<project>
    set(INSTALL_BIN       "usrd/bin")                    # /usr/[local]/bin
    set(INSTALL_INIT      "/etc/init.d")              # /etc/init (upstart init scripts)
    # set(INSTALL_DOC       "share/doc/${project}")   # /usr/[local]/share/doc/<project>        
else()
    # Install into local directory
    set(INSTALL_ROOT      ".")                      # ./
    set(INSTALL_CMAKE     "cmake")                  # ./cmake
    set(INSTALL_EXAMPLES  ".")                      # ./
    set(INSTALL_DATA      ".")                      # ./
    set(INSTALL_BIN       ".")                      # ./
    set(INSTALL_SHARED    "lib")                    # ./lib
    set(INSTALL_LIB       "lib")                    # ./lib
    set(INSTALL_INCLUDE   "include")                # ./include
    set(INSTALL_DOC       "doc")                    # ./doc
    set(INSTALL_SHORTCUTS "misc")                   # ./misc
    set(INSTALL_ICONS     "misc")                   # ./misc
    set(INSTALL_INIT      "misc")                   # ./misc
endif()

# Set runtime path
set(CMAKE_SKIP_BUILD_RPATH            FALSE) # Add absolute path to all dependencies for BUILD
set(CMAKE_BUILD_WITH_INSTALL_RPATH    FALSE) # Use CMAKE_INSTALL_RPATH for INSTALL
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE) # Do NOT add path to dependencies for INSTALL

if (NOT SYSTEM_DIR_INSTALL)
    # Find libraries relative to binary
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@loader_path/../../../${INSTALL_LIB}")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/${INSTALL_LIB}")
    endif()
endif()

set(DAEMON_MANAGER SysV)  # TODO SysV detection
if(EXISTS "/etc/systemd/")
    message(STATUS "Init: systemd detected")
    set(DAEMON_MANAGER systemd)
elseif (EXISTS "/etc/rc.conf")
    message(STATUS "Init: OpenRC detected")
    set(DAEMON_MANAGER OpenRC)
else()
    message(FATAL_ERROR "Neither systemd nor OpenRC detected - other init systems are not supported!")
endif()

# Found no other way to test it - but this one works well I guess
try_run (gpiocxx_FOUND gpiocxx_comp ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_check/check_gpio_dev.cpp)
try_run (i2cxx_FOUND i2ccxx_comp ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_check/check_i2c_dev.cpp)
if(gpiocxx_FOUND)
    message(STATUS "Found /dev/gpiochip internal dev-gpio library available")
else()
    message("/dev/gpiochipX not found internal dev-gpio library is not available - you either using old kernel that does not support GPIO device access, or device that does not support GPIO or you are missing GPIO driver (module)")
endif()
if(i2cxx_FOUND)
    message(STATUS "Found /dev/i2c-X internal dev-i2c library available")
else()
    message("/dev/i2c-X not found internal dev-i2c library is not available - you either missing module i2c-dev or you are missing i2c bus or some device specific driver to support i2c")
endif()

# External Libs detect
#
find_package (SQLite3)
find_package (Mosquitto)
find_package (JSON-C)
find_package (Config++)
find_package (pigpio)
find_package (udev)

set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/lua-5.3.4/src")
if (NOT CONFIG++_FOUND)
	message(SEND_ERROR "libconfig/libconfig++ not found - it is necessary requirement to read config files by mq_system")
endif()
if (NOT MOSQUITTO_FOUND)
    message(SEND_ERROR "(lib)mosquitto not found - it is necessary requirement to communicate with MQTT broker")
endif()
if (NOT JSON-C_FOUND)
    message(SEND_ERROR "(lib)json-c not found - it is necessary requirement to translate messages")
endif()

# Project modules 
add_subdirectory(extern)
add_subdirectory(source)
