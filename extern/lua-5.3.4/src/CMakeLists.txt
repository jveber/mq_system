# mq project file addition to standard lua
set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    add_definitions( -D_CRT_SECURE_NO_WARNINGS )
endif()

if (UNIX)
    add_definitions( -DLUA_USE_POSIX -DLUA_USE_DLOPEN )
endif()

if (APPLE)
    add_definitions( -DLUA_USE_MACOSX )
endif()

set(target lualib)

set(lua_core 
    lapi.c 
    lcode.c 
    lctype.c 
    ldebug.c 
    ldo.c 
    ldump.c 
    lfunc.c
    lgc.c 
    llex.c
    lmem.c 
    lobject.c 
    lopcodes.c 
    lparser.c
    lstate.c 
    lstring.c
    ltable.c
    ltm.c 
    lundump.c
    lvm.c 
    lzio.c
)
# make it all C++ to get exception handling instead of longjump
set_source_files_properties(lapi.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lcode.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lctype.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ldebug.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ldo.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ldump.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lfunc.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lgc.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(llex.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lmem.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lobject.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lopcodes.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lparser.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lstate.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lstring.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ltable.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ltm.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lundump.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lvm.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lzio.c PROPERTIES LANGUAGE CXX)

set(lua_lib
    lauxlib.c
    lbaselib.c
    lbitlib.c
    lcorolib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loslib.c
    lstrlib.c
    ltablib.c
    lutf8lib.c
    loadlib.c
    linit.c
)

# make it all C++ to get exception handling instead of longjump
set_source_files_properties(lauxlib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lbaselib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lbitlib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lcorolib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ldblib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(liolib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lmathlib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(loslib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lstrlib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(ltablib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(lutf8lib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(loadlib.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(linit.c PROPERTIES LANGUAGE CXX)

add_library (${target} STATIC ${lua_core} ${lua_lib})

if (${IPO_SUPPORTED})
	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()
