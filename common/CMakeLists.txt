set(GALERA_COMMON_LIB galera-common)

set(GALERA_COMMON_LIB_SRC
    filter.cpp
    fetch-hint.cpp
    sort-clause.cpp
    source.cpp
    vcard-parser.cpp
)

set(GALERA_COMMON_LIB_HEADERS
    filter.h
    fetch-hint.h
    sort-clause.h
    source.h
    vcard-parser.h
    dbus-service-defs.h
)

add_library(${GALERA_COMMON_LIB} STATIC
    ${GALERA_COMMON_LIB_SRC}
    ${GALERA_COMMON_LIB_HEADERS}
)

target_link_libraries(${GALERA_COMMON_LIB}
    Qt5::Core
    Qt5::Versit
    Qt5::Contacts
)
