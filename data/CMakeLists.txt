configure_file("${CMAKE_CURRENT_SOURCE_DIR}/com.canonical.galera.service.in"
               "${CMAKE_CURRENT_BINARY_DIR}/com.canonical.galera.service"
               IMMEDIATE @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.galera.service
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbus-1/services/
)
