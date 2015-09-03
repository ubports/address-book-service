configure_file(address-book-service.conf.in ${CMAKE_CURRENT_BINARY_DIR}/address-book-service.conf)
configure_file(address-book-update.conf.in ${CMAKE_CURRENT_BINARY_DIR}/address-book-update.conf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/address-book-service.conf
              ${CMAKE_CURRENT_BINARY_DIR}/address-book-update.conf
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions/)

