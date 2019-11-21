
# Target name
set(target mq_unipi_daemon)

if (pigpio_FOUND)
    set(sources ${target}_pi.cpp)
    add_executable(${target} ${sources} $<TARGET_OBJECTS:mq_lib>)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_link_libraries(${target} ${MOSQUITTO_LIBRARIES} ${CONFIG++_LIBRARY} ${SQLITE3_LIBRARIES} ${JSON-C_LIBRARIES} pthread ${pigpiod_if2_LIBRARY})
    cotire(${target})
elseif(gpiocxx_FOUND AND i2cxx_FOUND)
    set(sources ${target}.cpp )
    add_executable(${target} ${sources} $<TARGET_OBJECTS:mq_lib>)
    target_link_libraries(${target} ${MOSQUITTO_LIBRARIES} ${CONFIG++_LIBRARY} ${SQLITE3_LIBRARIES} ${JSON-C_LIBRARIES} pthread gpioxx i2cxx)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
    set_property(TARGET ${target} PROPERTY CMAKE_CXX_STANDARD_REQUIRED yes)
    cotire(${target})
else()
    message(SNED_ERROR "Neither pigpio nor gpiocxx found ... dunno what to link to unipi daemon")  # this should never happen
endif()

install(TARGETS ${target} RUNTIME DESTINATION /usr/local/bin)
install(FILES ${CMAKE_SOURCE_DIR}/data/${target}.conf DESTINATION /etc/mq_system/ )

if (DAEMON_MANAGER EQUAL 1)
    install(FILES ${CMAKE_SOURCE_DIR}/data/${target} PERMISSIONS WORLD_EXECUTE DESTINATION /etc/init.d/ )
    add_custom_target(uninstall_${target}
        COMMAND rm -f /usr/local/bin/${target}
        COMMAND rm -f /etc/init.d/${target}
        COMMAND rm -f /etc/mq_system/${target}.conf
    )
elseif (DAEMON_MANAGER EQUAL 2)
    install(FILES ${CMAKE_SOURCE_DIR}/data/${target}.service PERMISSIONS WORLD_READ DESTINATION /usr/lib/systemd/system/ )
    add_custom_target(uninstall_${target}
        COMMAND rm -f /usr/local/bin/${target}
        COMMAND rm -f /usr/lib/systemd/system/${target}.service
        COMMAND rm -f /etc/mq_system/${target}.conf
    )
else()
    message(SEND_ERROR "Unsupported deamon manager")  # this should never happen
endif()

add_dependencies(uninstall uninstall_${target})