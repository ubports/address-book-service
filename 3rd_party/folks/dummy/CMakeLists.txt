# Remove any link flag to keep it compatible with folks, otherwise folks would not be able to load it
set(CMAKE_MODULE_LINKER_FLAGS  "")

add_subdirectory(lib)
add_subdirectory(backend)
