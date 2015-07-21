find_program(EVOLUTION_ADDRESSBOOK_FACTORY_BIN evolution-addressbook-factory
             PATHS /usr/lib/evolution/)
add_subdirectory(data)
add_subdirectory(unittest)
