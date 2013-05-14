set(VCARD_UTILS_LIB vcard-utils)

set(VCARD_UTILS_LIB_SRC
    vcard-parser.cpp
)

set(VCARD_UTILS_LIB_HEADERS
    vcard-parser.h
)

add_library(${VCARD_UTILS_LIB} STATIC
    ${VCARD_UTILS_LIB_SRC}
    ${VCARD_UTILS_LIB_HEADERS}
)

qt5_use_modules(${VCARD_UTILS_LIB} Core Versit)

