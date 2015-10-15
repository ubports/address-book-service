add_custom_target(copy_accounts_full_db ALL)
add_custom_command(TARGET copy_accounts_full_db PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/accounts.db ${CMAKE_CURRENT_BINARY_DIR}/)

if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    add_dependencies(copydbfiles copy_accounts_full_db)
endif()
