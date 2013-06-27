macro(declare_test TESTNAME)
    add_executable(${TESTNAME}
                    ${TESTNAME}.cpp
    )
    qt5_use_modules(${TESTNAME} Core Contacts Versit Test)

    if(TEST_XML_OUTPUT)
        set(TEST_ARGS -p -xunitxml -p -o -p test_${testname}.xml)
    else()
        set(TEST_ARGS "")
    endif()

    target_link_libraries(${TESTNAME}
                          address-book-service-lib
                          ${GLIB_LIBRARIES}
                          ${GIO_LIBRARIES}
                          ${FOLKS_LIBRARIES}
    )

    set(FOLKS_ENVIROMENT "FOLKS_BACKEND_STORE_KEY_FILE_PATH=${FOLKS_BACKEND_CONFIG_FILE};FOLKS_BACKEND_KEY_FILE_PATH=${FOLKS_KEY_FILE_DATA_FILE}")
    add_test(${TESTNAME} ${TESTNAME})
    set_tests_properties(${TESTNAME} PROPERTIES
                         TIMEOUT ${CTEST_TESTING_TIMEOUT}
                         ENVIRONMENT "QT_QPA_PLATFORM=minimal;${FOLKS_ENVIROMENT}")
endmacro(declare_test testname)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
)

add_definitions(-DTEST_SUITE)
if(NOT CTEST_TESTING_TIMEOUT)
    set(CTEST_TESTING_TIMEOUT 60)
endif()

declare_test(clause-test)
declare_test(contactmap-test)
declare_test(sort-clause-test)
declare_test(fetch-hint-test)
