configure_file(address-book-service.conf.in ${CMAKE_CURRENT_BINARY_DIR}/address-book-service.conf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/address-book-service.conf
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions/)

