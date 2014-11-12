find_program(EVOLUTION_ADDRESSBOOK_FACTORY_BIN evolution-addressbook-factory
             PATHS /usr/lib/evolution/)
set(EVOLUTION_ADDRESSBOOK_SERVICE_NAME "org.gnome.evolution.dataserver.AddressBook6")
add_subdirectory(data)
add_subdirectory(unittest)
