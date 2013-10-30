project(address-book-service-lib)

set(CONTACTS_SERVICE_LIB address-book-service-lib)

set(CONTACTS_SERVICE_LIB_SRC
    addressbook.cpp
    addressbook-adaptor.cpp
    contacts-map.cpp
    detail-context-parser.cpp
    gee-utils.cpp
    qindividual.cpp
    source.cpp
    update-contact-request.cpp
    view.cpp
    view-adaptor.cpp
)

set(CONTACTS_SERVICE_LIB_HEADERS
    addressbook.h
    addressbook-adaptor.h
    contacts-map.h
    detail-context-parser.h
    gee-utils.h
    qindividual.h
    source.h
    update-contact-request.h
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

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)
