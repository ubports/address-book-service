set(GALERA_COMMON_LIB galera-common)

set(GALERA_COMMON_LIB_SRC
    contacts-map.cpp
    filter.cpp
    qindividual.cpp
    sort-clause.cpp
    vcard-parser.cpp
)

set(GALERA_COMMON_LIB_HEADERS
    contacts-map.h
    filter.h
    qindividual.h
    sort-clause.h
    vcard-parser.h
)

add_library(${GALERA_COMMON_LIB} STATIC
    ${GALERA_COMMON_LIB_SRC}
    ${GALERA_COMMON_LIB_HEADERS}
)

target_link_libraries(${GALERA_COMMON_LIB}
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
)

include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

qt5_use_modules(${GALERA_COMMON_LIB} Core Versit Contacts)
