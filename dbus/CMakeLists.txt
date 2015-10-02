configure_file(com.canonical.pim.updater.service.in ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.updater.service)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.pim.updater.service
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/services/)

