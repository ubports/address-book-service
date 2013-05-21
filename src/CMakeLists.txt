set(CONTACTS_SERVICE_LIB galera-contact-service-lib)

set(CONTACTS_SERVICE_LIB_SRC
    addressbook.cpp
    addressbook-adaptor.cpp
    contacts-map.cpp
    qindividual.cpp
    source.cpp
    view.cpp
    view-adaptor.cpp
)

set(CONTACTS_SERVICE_LIB_HEADERS
    addressbook.h
    addressbook-adaptor.h
    contacts-map.h
    qindividual.h
    source.h
    view.h
    view-adaptor.h
)

add_library(${CONTACTS_SERVICE_LIB} STATIC
    ${CONTACTS_SERVICE_LIB_SRC}
    ${CONTACTS_SERVICE_LIB_HEADERS}
)

target_link_libraries(${CONTACTS_SERVICE_LIB}
    galera-common
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
)

qt5_use_modules(${CONTACTS_SERVICE_LIB} Core Contacts DBus Versit)

set(CONTACTS_SERVICE galera-contact-service)

add_executable(${CONTACTS_SERVICE}
    main.cpp
)

target_link_libraries(${CONTACTS_SERVICE}
    ${CONTACTS_SERVICE_LIB}
)

qt5_use_modules(${CONTACTS_SERVICE} Core Contacts DBus Versit)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

install(TARGETS ${CONTACTS_SERVICE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_LIBEXECDIR})

