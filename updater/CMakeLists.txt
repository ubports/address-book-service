set(ADDRESS_BOOK_UPDATE_LIB address-book-update-lib)
set(ADDRESS_BOOK_UPDATE_BIN address-book-update)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${AccountsQt5_INCLUDE_DIRS}
)

set(ADDRESS_BOOK_UPDATE_LIB_SRCS
    ab-update.h
    ab-update.cpp
    ab-update-adaptor.h
    ab-update-adaptor.cpp
    ab-update-buteo-import.h
    ab-update-buteo-import.cpp
    ab-update-module.h
    ab-update-module.cpp
)

add_library(${ADDRESS_BOOK_UPDATE_LIB} STATIC
    ${ADDRESS_BOOK_UPDATE_LIB_SRCS}
)

target_link_libraries(${ADDRESS_BOOK_UPDATE_LIB}
    Qt5::Core
    Qt5::DBus
    Qt5::Contacts
    Qt5::Xml
    Qt5::Network
    ${AccountsQt5_LIBRARIES}
)

add_executable(${ADDRESS_BOOK_UPDATE_BIN}
    main.cpp
)

target_link_libraries(${ADDRESS_BOOK_UPDATE_BIN}
    ${ADDRESS_BOOK_UPDATE_LIB}
)

install(TARGETS ${ADDRESS_BOOK_UPDATE_BIN}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
