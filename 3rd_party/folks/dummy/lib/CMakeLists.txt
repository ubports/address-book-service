project(folks-dummy-lib)

vala_precompile(LIB_DUMMY_VALA_C
SOURCES
    internal.vala
    dummy-backend.vala
    dummy-fat-persona.vala
    dummy-persona-store.vala
    dummy-persona.vala
PACKAGES
    posix
    folks
    gee-0.8
    gio-2.0
    gobject-2.0
)

add_definitions(-DBACKEND_NAME="dummy")
add_definitions(-fPIC)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

add_library(folks-dummy STATIC
    ${LIB_DUMMY_VALA_C}
)

target_link_libraries(folks-dummy
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${FOLKS_LIBRARIES}
)
