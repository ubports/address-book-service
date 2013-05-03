set(CONTACTS_APP galera)

set(CONTACTS_APP_SRC
    main.cpp
    addressbook.cpp
    addressbook-adaptor.cpp
    contacts-map.cpp
    qindividual.cpp
    source.cpp
    view.cpp
    view-adaptor.cpp
)

set(CONTACTS_APP_HEADERS
    addressbook.h
    addressbook-adaptor.h
    contacts-map.h
    qindividual.h
    source.h
    view.h
    view-adaptor.h
)

add_executable(${CONTACTS_APP}
    ${CONTACTS_APP_SRC}
    ${CONTACTS_APP_HEADERS}
)

target_link_libraries(${CONTACTS_APP}
    ${GLIB_LIBRARIES}
    ${FOLKS_LIBRARIES}
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

qt5_use_modules(${CONTACTS_APP} Core DBus Versit)

install(TARGETS ${CONTACTS_APP}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

