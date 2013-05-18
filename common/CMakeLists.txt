set(GALERA_COMMON_LIB galera-common)

set(GALERA_COMMON_LIB_SRC
    filter.cpp
    vcard-parser.cpp
)

set(GALERA_COMMON_LIB_HEADERS
    filter.h
    vcard-parser.h
)

add_library(${GALERA_COMMON_LIB} STATIC
    ${GALERA_COMMON_LIB_SRC}
    ${GALERA_COMMON_LIB_HEADERS}
)

qt5_use_modules(${GALERA_COMMON_LIB} Core Versit Contacts)
