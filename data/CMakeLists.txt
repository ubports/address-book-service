install(FILES com.canonical.pim.AddressBook.gschema.xml
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/glib-2.0/schemas/)

find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
install(CODE "execute_process(COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_INSTALL_FULL_DATADIR}/glib-2.0/schemas/)")

