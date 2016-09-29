configure_file(com.canonical.pim.updater.service.in 
    ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.updater.service)

configure_file(com.canonical.pim.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.service)

install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.updater.service
    ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.service
DESTINATION
     ${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/services/)
