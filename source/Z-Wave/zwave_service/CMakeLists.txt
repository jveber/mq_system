# Target name
set(target mq_zwave_daemon)

set(sources ${target}.cpp)

add_executable(${target} ${sources} $<TARGET_OBJECTS:mq_lib>)
target_link_libraries(${target} ${MOSQUITTO_LIBRARIES} ${JSON-C_LIBRARIES} ${OpenZWave} ${UDEV_LIBRARIES} ${CONFIG++_LIBRARY} ${SQLITE3_LIBRARIES} pthread resolv)
set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)
if (${IPO_SUPPORTED})
	set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()



install(TARGETS ${target} RUNTIME DESTINATION /usr/local/bin)
if (DAEMON_MANAGER EQUAL 1)
    install(FILES ${CMAKE_SOURCE_DIR}/data/${target} PERMISSIONS WORLD_EXECUTE DESTINATION /etc/init.d/ )
elseif (DAEMON_MANAGER EQUAL 2)
    install(FILES ${CMAKE_SOURCE_DIR}/data/${target}.service PERMISSIONS WORLD_READ DESTINATION /usr/lib/systemd/system/ )
else()
    message(SEND_ERROR "Unsupported deamon manager")  # this should never happen
endif()
install(FILES ${CMAKE_SOURCE_DIR}/data/${target}.conf DESTINATION /etc/mq_system/ )
cotire(${target}) 
if (DAEMON_MANAGER EQUAL 1)
    add_custom_target(uninstall_${target}
	    COMMAND rm -f /usr/local/bin/${target}
        COMMAND rm -f /etc/init.d/${target}
        COMMAND rm -f /etc/mq_system/${target}.conf
    )
elseif (DAEMON_MANAGER EQUAL 2)
    add_custom_target(uninstall_${target}
	    COMMAND rm -f /usr/local/bin/${target}
        COMMAND rm -f /usr/lib/systemd/system/${target}.service
        COMMAND rm -f /etc/mq_system/${target}.conf
    )
else()
    message(SEND_ERROR "Unsupported deamon manager")  # this should never happen
endif()

add_dependencies(uninstall uninstall_${target})

