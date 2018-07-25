project(qtcontacts_galera)

set(QCONTACTS_BACKEND qtcontacts_galera)

set(QCONTACTS_BACKEND_SRCS
    qcontact-backend.cpp
    qcontactfetchrequest-data.cpp
    qcontactfetchbyidrequest-data.cpp
    qcontactremoverequest-data.cpp
    qcontactrequest-data.cpp
    qcontactsaverequest-data.cpp
    contacts-service.cpp
)

set(QCONTACTS_BACKEND_HDRS
    qcontact-backend.h
    qcontactfetchrequest-data.h
    qcontactfetchbyidrequest-data.h
    qcontactremoverequest-data.h
    qcontactrequest-data.h
    qcontactsaverequest-data.h
    contacts-service.h
)

add_library(${QCONTACTS_BACKEND} SHARED
    ${QCONTACTS_BACKEND_SRCS}
    ${QCONTACTS_BACKEND_HDRS}
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(${QCONTACTS_BACKEND}
    galera-common
    Qt5::Core
    Qt5::Contacts
    Qt5::DBus
    Qt5::Versit
)

add_definitions(-std=gnu++11)

execute_process(
    COMMAND qmake -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS ${QCONTACTS_BACKEND} LIBRARY DESTINATION ${QT_INSTALL_PLUGINS}/contacts)
