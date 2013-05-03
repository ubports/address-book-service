set(QCONTACTS_BACKEND qtcontacts_galera)

set(QCONTACTS_BACKEND_SRCS
    qcontact-backend.cpp
    qcontact-engineid.cpp
    contacts-service.cpp
)

set(QCONTACTS_BACKEND_HDRS
    qcontact-backend.h
    qcontact-engineid.h
    contacts-service.h
)

add_library(${QCONTACTS_BACKEND} MODULE ${QCONTACTS_BACKEND_SRCS} ${QCONTACTS_BACKEND_HDRS})
qt5_use_modules(${QCONTACTS_BACKEND} Core Contacts DBus Versit)
add_definitions(-std=gnu++11)


execute_process(
    COMMAND qmake -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS ${QCONTACTS_BACKEND} LIBRARY DESTINATION ${QT_INSTALL_PLUGINS}/contacts)
