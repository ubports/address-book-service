set(CONTACTS_SERVICE galera-contact-service)

set(CONTACTS_SERVICE_SRC
    main.cpp
    addressbook.cpp
    addressbook-adaptor.cpp
    contacts-map.cpp
    qindividual.cpp
    source.cpp
    view.cpp
    view-adaptor.cpp
)

set(CONTACTS_SERVICE_HEADERS
    addressbook.h
    addressbook-adaptor.h
    contacts-map.h
    qindividual.h
    source.h
    view.h
    view-adaptor.h
)

add_executable(${CONTACTS_SERVICE}
    ${CONTACTS_SERVICE_SRC}
    ${CONTACTS_SERVICE_HEADERS}
)

target_link_libraries(${CONTACTS_SERVICE}
    galera-common
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

qt5_use_modules(${CONTACTS_SERVICE} Core Contacts DBus Versit)

install(TARGETS ${CONTACTS_SERVICE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_LIBEXECDIR})

