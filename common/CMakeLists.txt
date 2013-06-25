set(GALERA_COMMON_LIB galera-common)

set(GALERA_COMMON_LIB_SRC
    filter.cpp
    fetch-hint.cpp
    sort-clause.cpp
    vcard-parser.cpp
)

set(GALERA_COMMON_LIB_HEADERS
    filter.h
    fetch-hint.h
    sort-clause.h
    vcard-parser.h
    dbus-service-defs.h
)

add_library(${GALERA_COMMON_LIB} STATIC
    ${GALERA_COMMON_LIB_SRC}
    ${GALERA_COMMON_LIB_HEADERS}
)

qt5_use_modules(${GALERA_COMMON_LIB} Core Versit Contacts)
