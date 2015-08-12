project(address-book-service-lib)

set(CONTACTS_SERVICE_LIB address-book-service-lib)

set(CONTACTS_SERVICE_LIB_SRC
    addressbook.cpp
    addressbook-adaptor.cpp
    contact-less-than.cpp
    contacts-map.cpp
    detail-context-parser.cpp
    dirtycontact-notify.cpp
    gee-utils.cpp
    qindividual.cpp
    update-contact-request.cpp
    view.cpp
    view-adaptor.cpp
)

set(CONTACTS_SERVICE_LIB_HEADERS
    addressbook.h
    addressbook-adaptor.h
    contact-less-than.h
    contacts-map.h
    detail-context-parser.h
    dirtycontact-notify.h
    gee-utils.h
    qindividual.h
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
    ubuntu-source-eds
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
    ${FOLKS_EDS_LIBRARIES}
    ${LibPhoneNumber_LIBRARIES}
    Qt5::Core
    Qt5::Contacts
    Qt5::DBus
    Qt5::Versit
)

include_directories(
    ${ubuntu-sources_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
    ${FOLKS_EDS_INCLUDE_DIRS}
    ${LibPhoneNumber_INCLUDE_DIRS}
)
