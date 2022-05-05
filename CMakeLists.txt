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
enable_language(C CXX)
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_INFO)
set(CMAKE_UNITY_BUILD  YES)
message(STATUS "Using IPO ${IPO_SUPPORTED} info ${IPO_INFO}")

include(CMakeDependentOption)



set(WriterCompilerDetectionHeaderFound NOTFOUND)
include(WriteCompilerDetectionHeader OPTIONAL RESULT_VARIABLE WriterCompilerDetectionHeaderFound)

# Include custom cmake modules
include(cmake/GetGitRevisionDescription.cmake)
#include(cmake/cotire.cmake)
include(CMakeDependentOption)

set (COTIRE_MINIMUM_NUMBER_OF_TARGET_SOURCES 1000)
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
set(CMAKE_C_FLAGS "-march=native -mfpu=neon-vfpv4 -mfloat-abi=hard -mtune=native -fpie -Wl,-pie -Wall -Wextra -pipe -fno-common  -fno-plt -fno-omit-frame-pointer -static-libstdc++ -lrt")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" )
set(C_RELEASE_FLAGS "-DNDEBUG -O2  -Wno-unused -fasynchronous-unwind-tables -fno-semantic-interposition -fstack-clash-protection -fstack-protector-strong -fipa-pta -fdevirtualize-at-ltrans -ftree-vectorize -Wno-reorder -fgraphite-identity -floop-nest-optimize -ftree-loop-distribution -fstrict-aliasing")
# -fsanitize=memory -fsanitize-memory-track-origins -fsanitize=thread -fsanitize=undefined -fsanitize=float-cast-overflow -fsanitize=pointer-compare -fsanitize=float-divide-by-zero -fsanitize=pointer-subtract -D_GLIBCXX_SANITIZE_VECTOR -fsanitize-address-use-after-scope -U_FORTIFY_SOURCE -Wdouble-promotion -Wno-psabi -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Werror=format-security -Werror=implicit-function-declaration -U_FORTIFY_SOURCE -fsanitize=address
# -fsanitize=thread -fsanitize=memory -fsanitize-memory-track-origins
string(CONCAT C_DEBUG_FLAGS "-O0 -g3 -fno-omit-frame-pointer -Wpedantic -fexceptions -fsanitize-recover=all -Wl,-z,defs -Wl,-z,now -Wl,-z,relro -Wformat=2 -fno-strict-aliasing -Wstrict-aliasing=2"
#" -fsanitize=thread"
" -fsanitize=address"
" -fsanitize-address-use-after-scope"
" -fsanitize=bool"
" -fsanitize=bounds"
" -fsanitize=bounds-strict"
" -fsanitize=builtin"
" -fsanitize=enum"
" -fsanitize=float-cast-overflow"
" -fsanitize=float-divide-by-zero"
" -fsanitize=integer-divide-by-zero"
" -fsanitize=nonnull-attribute"
" -fsanitize=null"
" -fsanitize=object-size"
" -fsanitize=pointer-compare"
" -fsanitize=pointer-subtract"
" -fsanitize=pointer-overflow"
" -fsanitize=return"
" -fsanitize=shift"
" -fsanitize=shift-exponent"
" -fsanitize=shift-base"
" -fsanitize=signed-integer-overflow"
" -fsanitize=undefined"
" -fsanitize=unreachable"
" -fsanitize=vla-bound"
" -fsanitize=vptr"
" -D_GLIBCXX_SANITIZE_VECTOR -Wdouble-promotion -Wno-psabi -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Werror=format-security")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${C_DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${C_RELEASE_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Wjump-misses-init ${C_DEBUG_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} ${C_RELEASE_FLAGS}")

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(IPO_SUPPORTED OFF)  # only for DEBUG
endif()
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

# managers:
#   0 - SysV
#   1 - OpenRC
#   2 - systemd

set(DAEMON_MANAGER 0)  # TODO SysV detection
if(EXISTS "/bin/systemctl")
    message(STATUS "Init: systemd detected")
    set(DAEMON_MANAGER 2)
elseif (EXISTS "/etc/rc.conf")
    message(STATUS "Init: OpenRC detected")
    set(DAEMON_MANAGER 1)
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

set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/lua-5.4.4/src")

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
