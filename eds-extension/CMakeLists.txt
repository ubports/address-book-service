project(ubuntu-sources)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GLIB_ACCOUNTS_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_EDS_INCLUDE_DIRS}
)

execute_process(
    COMMAND pkg-config --variable=moduledir libebackend-1.2
    OUTPUT_VARIABLE EDS_MODULES_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Ubuntu Source
set(UBUNTU_SOURCE_LIB ubuntu-source-eds)
set(UBUNTU_SOURCE_LIB_SRCS
    e-source-ubuntu.c
    e-source-ubuntu.h
)
add_library(${UBUNTU_SOURCE_LIB} SHARED
    ${UBUNTU_SOURCE_LIB_SRCS}
)
target_link_libraries(${UBUNTU_SOURCE_LIB}
    ${GLIB_LIBRARIES}
    ${GLIB_ACCOUNTS_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_EDS_LIBRARIES}
)
install(TARGETS ${UBUNTU_SOURCE_LIB}
    LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

# Module Ubuntu Sources
set(MODULE_UBUNTU_SOURCES  module-ubuntu-sources)
set(MODULE_UBUNTU_SOURCES_SRCS
    module-ubuntu-sources.c
)
add_library(${MODULE_UBUNTU_SOURCES} MODULE
    ${MODULE_UBUNTU_SOURCES_SRCS}
)
set_target_properties(${MODULE_UBUNTU_SOURCES}
    PROPERTIES PREFIX ""
)
target_link_libraries(${MODULE_UBUNTU_SOURCES}
    ${GLIB_LIBRARIES}
    ${GLIB_ACCOUNTS_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_EDS_LIBRARIES}
    ${UBUNTU_SOURCE_LIB}
)
install(TARGETS ${MODULE_UBUNTU_SOURCES}
    LIBRARY DESTINATION ${EDS_MODULES_DIR}/
)
