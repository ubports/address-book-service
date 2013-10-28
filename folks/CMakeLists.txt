list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/vala)

find_package(Vala "0.17.6" REQUIRED)
include(UseVala)

add_subdirectory(dummy)
