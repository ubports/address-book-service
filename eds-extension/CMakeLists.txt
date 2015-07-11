project(ubuntu-sources)

set(MODULE_UBUNTU_SOURCE ubuntu-sources-eds)

set(MODULE_UBUNTU_SOURCE_SRCS
    module-ubuntu-sources.c
)

add_library(${MODULE_UBUNTU_SOURCE} MODULE
    ${MODULE_UBUNTU_SOURCE_SRCS}
)

set_target_properties(${MODULE_UBUNTU_SOURCE}
    PROPERTIES PREFIX ""
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GLIB_ACCOUNTS_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_EDS_INCLUDE_DIRS}
)

target_link_libraries(${MODULE_UBUNTU_SOURCE}
    ${GLIB_LIBRARIES}
    ${GLIB_ACCOUNTS_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_EDS_LIBRARIES}
)

execute_process(
    COMMAND pkg-config --variable=moduledir libebackend-1.2
    OUTPUT_VARIABLE EDS_MODULES_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS ${MODULE_UBUNTU_SOURCE}
    LIBRARY DESTINATION ${EDS_MODULES_DIR}/
)
