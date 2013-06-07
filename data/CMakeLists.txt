configure_file("${CMAKE_CURRENT_SOURCE_DIR}/com.canonical.pim.service.in"
               "${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.service"
               IMMEDIATE @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.service
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbus-1/services/
)
