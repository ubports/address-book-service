project(folks-dummy-backend)

vala_precompile(DUMMY_VALA_C
SOURCES
    dummy-backend-factory.vala
PACKAGES
    posix
    folks
    gee-0.8
    gio-2.0
    gobject-2.0
CUSTOM_VAPIS
    ${folks-dummy-lib_SOURCE_DIR}/folks-dummy.vapi
)

add_definitions(-DBACKEND_NAME="dummy")

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${folks-dummy-lib_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

add_library(dummy MODULE
    ${DUMMY_VALA_C}
)

set_target_properties(dummy
PROPERTIES
    PREFIX ""
)

target_link_libraries(dummy
    folks-dummy
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
)



